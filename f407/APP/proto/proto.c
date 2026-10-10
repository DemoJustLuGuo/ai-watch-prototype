#include "proto.h"

/* Modbus CRC16：多项式 0xA001，初值 0xFFFF。覆盖 VER 到载荷末字节。
   CRC 可续算：把中间值当种子继续喂字节，与一次算完等价。 */
static u16 crc16_update(u16 crc, const u8 *data, u16 len)
{
	u16 i;
	u8 j;

	for(i=0;i<len;i++)
	{
		crc^=data[i];
		for(j=0;j<8;j++)
		{
			if(crc&0x0001) crc=(crc>>1)^0xA001;
			else crc>>=1;
		}
	}
	return crc;
}

u16 proto_crc16(const u8 *data, u16 len)
{
	return crc16_update(0xFFFF, data, len);
}

u16 proto_build(u8 *out, u8 type, u8 seq, const u8 *payload, u16 len)
{
	u16 crc;
	u16 i;

	if(len>PROTO_MAX_PAYLOAD) len=PROTO_MAX_PAYLOAD;

	out[0]=PROTO_SOF0;
	out[1]=PROTO_SOF1;
	out[2]=PROTO_VER;
	out[3]=PROTO_SRC_F4;
	out[4]=PROTO_SRC_C3;
	out[5]=type;
	out[6]=seq;
	out[7]=(u8)(len&0xFF);		//LEN 小端
	out[8]=(u8)(len>>8);
	for(i=0;i<len;i++) out[PROTO_HEAD_LEN+i]=payload[i];

	crc=proto_crc16(&out[2], (u16)(7+len));	//VER..载荷末字节
	out[PROTO_HEAD_LEN+len]  =(u8)(crc&0xFF);	//CRC 小端
	out[PROTO_HEAD_LEN+len+1]=(u8)(crc>>8);
	return (u16)(PROTO_HEAD_LEN+len+2);
}

/* 接收状态机：逐字节找 SOF，LEN 合法才收载荷，最后校验 CRC。 */
u8 proto_feed(u8 byte, proto_frame_t *f, u8 *crc_err)
{
	static u8  st=0;		//0..1 找SOF；2 VER；3 SRC；4 DST；5 TYPE；6 SEQ；7..8 LEN；9 载荷；10..11 CRC
	static u8  ver,src,dst,type,seq;
	static u16 len,idx;
	static u8  buf[PROTO_MAX_PAYLOAD];
	static u16 rx_crc;

	*crc_err=0;

	switch(st)
	{
	case 0:
		if(byte==PROTO_SOF0) st=1;
		break;
	case 1:
		if(byte==PROTO_SOF1) st=2;
		else if(byte==PROTO_SOF0) st=1;	//连续 A5 时停在这
		else st=0;
		break;
	case 2: ver=byte; st=3; break;
	case 3: src=byte; st=4; break;
	case 4: dst=byte; st=5; break;
	case 5: type=byte; st=6; break;
	case 6: seq=byte; st=7; break;
	case 7: len=byte; st=8; break;			//LEN 低字节
	case 8:
		len|=(u16)byte<<8;					//LEN 高字节
		if(len>PROTO_MAX_PAYLOAD) { st=0; break; }	//非法长度，丢弃
		idx=0;
		st=(len>0)?9:10;
		break;
	case 9:
		buf[idx++]=byte;
		if(idx>=len) st=10;
		break;
	case 10: rx_crc=byte; st=11; break;		//CRC 低字节
	case 11:
		{
			u8 head[7];
			u16 calc;
			rx_crc|=(u16)byte<<8;			//CRC 高字节
			st=0;
			head[0]=ver; head[1]=src; head[2]=dst;
			head[3]=type; head[4]=seq;
			head[5]=(u8)(len&0xFF); head[6]=(u8)(len>>8);
			calc=crc16_update(0xFFFF, head, 7);	//先算头
			calc=crc16_update(calc, buf, len);	//续算载荷
			if(calc!=rx_crc) { *crc_err=1; return 0; }
			f->src=src; f->dst=dst; f->type=type; f->seq=seq; f->len=len;
			for(idx=0;idx<len;idx++) f->payload[idx]=buf[idx];
			return 1;
		}
	default: st=0; break;
	}
	return 0;
}
