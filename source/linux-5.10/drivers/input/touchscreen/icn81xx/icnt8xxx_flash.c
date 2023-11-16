#define LOG_TAG         "Flash"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_firmware.h"
#include "icnt8xxx_flash.h"

struct file  *fp; 
int g_status = R_OK;
//static char fw_mode = 0;
static int fw_size = 0;
static unsigned char *fw_buf;
// junfuzhang 20160913, check crc in sram before boot
unsigned short icnt87_sram_crc = 0;
unsigned short icnt87_sram_length = 0;
//junfuzhang 20160913, adding end

static unsigned short const Crc16Table[]= {
    0x0000,0x8005,0x800F,0x000A,0x801B,0x001E,0x0014,0x8011,0x8033,0x0036,0x003C,0x8039,0x0028,0x802D,0x8027,0x0022, 
    0x8063,0x0066,0x006C,0x8069,0x0078,0x807D,0x8077,0x0072,0x0050,0x8055,0x805F,0x005A,0x804B,0x004E,0x0044,0x8041, 
    0x80C3,0x00C6,0x00CC,0x80C9,0x00D8,0x80DD,0x80D7,0x00D2,0x00F0,0x80F5,0x80FF,0x00FA,0x80EB,0x00EE,0x00E4,0x80E1, 
    0x00A0,0x80A5,0x80AF,0x00AA,0x80BB,0x00BE,0x00B4,0x80B1,0x8093,0x0096,0x009C,0x8099,0x0088,0x808D,0x8087,0x0082, 
    0x8183,0x0186,0x018C,0x8189,0x0198,0x819D,0x8197,0x0192,0x01B0,0x81B5,0x81BF,0x01BA,0x81AB,0x01AE,0x01A4,0x81A1, 
    0x01E0,0x81E5,0x81EF,0x01EA,0x81FB,0x01FE,0x01F4,0x81F1,0x81D3,0x01D6,0x01DC,0x81D9,0x01C8,0x81CD,0x81C7,0x01C2, 
    0x0140,0x8145,0x814F,0x014A,0x815B,0x015E,0x0154,0x8151,0x8173,0x0176,0x017C,0x8179,0x0168,0x816D,0x8167,0x0162, 
    0x8123,0x0126,0x012C,0x8129,0x0138,0x813D,0x8137,0x0132,0x0110,0x8115,0x811F,0x011A,0x810B,0x010E,0x0104,0x8101, 
    0x8303,0x0306,0x030C,0x8309,0x0318,0x831D,0x8317,0x0312,0x0330,0x8335,0x833F,0x033A,0x832B,0x032E,0x0324,0x8321, 
    0x0360,0x8365,0x836F,0x036A,0x837B,0x037E,0x0374,0x8371,0x8353,0x0356,0x035C,0x8359,0x0348,0x834D,0x8347,0x0342, 
    0x03C0,0x83C5,0x83CF,0x03CA,0x83DB,0x03DE,0x03D4,0x83D1,0x83F3,0x03F6,0x03FC,0x83F9,0x03E8,0x83ED,0x83E7,0x03E2, 
    0x83A3,0x03A6,0x03AC,0x83A9,0x03B8,0x83BD,0x83B7,0x03B2,0x0390,0x8395,0x839F,0x039A,0x838B,0x038E,0x0384,0x8381, 
    0x0280,0x8285,0x828F,0x028A,0x829B,0x029E,0x0294,0x8291,0x82B3,0x02B6,0x02BC,0x82B9,0x02A8,0x82AD,0x82A7,0x02A2, 
    0x82E3,0x02E6,0x02EC,0x82E9,0x02F8,0x82FD,0x82F7,0x02F2,0x02D0,0x82D5,0x82DF,0x02DA,0x82CB,0x02CE,0x02C4,0x82C1, 
    0x8243,0x0246,0x024C,0x8249,0x0258,0x825D,0x8257,0x0252,0x0270,0x8275,0x827F,0x027A,0x826B,0x026E,0x0264,0x8261, 
    0x0220,0x8225,0x822F,0x022A,0x823B,0x023E,0x0234,0x8231,0x8213,0x0216,0x021C,0x8219,0x0208,0x820D,0x8207,0x0202, 
};

static int icn85xx_update_status(int status)
{
//  cts_info("icn85xx_update_status: %d\n", status);
    g_status = status;
    return 0;
}

//static int icn85xx_get_status(void)
//{
//    return  g_status;
//}

static void icn85xx_set_fw(unsigned char *buf, int size)
{
    fw_size = size;
    fw_buf = buf;    
}

static int  icn85xx_open_fw( char *fw)
{
    return fw_size;
}

static int  icn85xx_read_fw(int offset, int length, char *buf)
{
    //loff_t  pos = offset;    
    
    memcpy(buf, fw_buf+offset, length);
    return 0;       
}

//static int  icn85xx_close_fw(void)
//{   
//    return 0;
//}
#if 0
static int icn85xx_readVersion(void)
{
    int err = 0;
    char tmp[2];    
    short CurVersion;
    
    if (err < 0) {
        cts_err("%s failed: %d\n", __func__, err); 
        return 0;
    }       
    CurVersion = (tmp[0]<<8) | tmp[1];
    return CurVersion;  
}

static void icn85xx_set_prog_addr(struct cts_device *cts_dev)
{
    //return cts_plat_reset_device(cts_dev->pdata);
}
#else
static int icn85xx_goto_progmode(struct cts_device *cts_dev)
{
    int ret = -1;
    int retry = 3;
    unsigned char ucTemp;  
    
    cts_info("icn85xx_goto_progmode..."); 
    while(retry > 0){       
//        icn85xx_set_prog_addr(cts_dev);
        ucTemp = 0x5a;               
        ret = icn85xx_prog_i2c_txdata(cts_dev,0xcc3355, &ucTemp,1);             
        if(ret < 0){
            retry--;
            mdelay(2); 
        }
        else
            break;
    }
    if(retry == 0)
        return -1;
    cts_info("icn85xx_goto_progmode over");
    return 0;
}
#endif

#if 0
static int icn85xx_check_progmod(struct cts_device *cts_dev)
{
    int ret;
    unsigned char ucTemp = 0x0;
    ret = icn85xx_prog_i2c_rxdata(cts_dev,0x040002, &ucTemp, 1);
//    cts_dbg("icn85xx_check_progmod: 0x%x", ucTemp);
    if(ret){
        cts_err("icn85xx_check_progmod error, ret: %d\n", ret);
        return ret;
    }
    if(ucTemp == 0x85)
        return 0;
    else 
        return -1;
}
#endif

static unsigned char FlashState(struct cts_device *cts_dev, unsigned char State_Index)
{
    unsigned char ucTemp[4] = {0,0,0,0};

    ucTemp[2]=0x08;
    ucTemp[1]=0x10;
    ucTemp[0]=0x00;
    icn85xx_prog_i2c_txdata(cts_dev,0x4062c,ucTemp,3);

    if(State_Index==0){
        ucTemp[0]=0x05;
    }else if(State_Index==1){
        ucTemp[0]=0x35;
    }
    icn85xx_prog_i2c_txdata(cts_dev,0x40630,ucTemp,1);

    ucTemp[1]=0x00;
    ucTemp[0]=0x01;
    icn85xx_prog_i2c_txdata(cts_dev,0x40640,ucTemp,2);
       
    ucTemp[0]=1;
    icn85xx_prog_i2c_txdata(cts_dev,0x40644,ucTemp,1);
    while(ucTemp[0]){
        icn85xx_prog_i2c_rxdata(cts_dev,0x40644,ucTemp,1);
    }
    icn85xx_prog_i2c_rxdata(cts_dev,0x40648,ucTemp,1);
    
    return (unsigned char)(ucTemp[0]);
}

int  icn85xx_read_flashid(struct cts_device *cts_dev)
{
    unsigned char ucTemp[4] = {0,0,0,0};
    int flashid=0;

    ucTemp[2]=0x08;
    ucTemp[1]=0x10;
    ucTemp[0]=0x00;
    icn85xx_prog_i2c_txdata(cts_dev,0x4062c,ucTemp,3);

    ucTemp[0]=0x9f;
    icn85xx_prog_i2c_txdata(cts_dev,0x40630,ucTemp,1);

    ucTemp[1]=0x00;
    ucTemp[0]=0x03;
    icn85xx_prog_i2c_txdata(cts_dev,0x40640,ucTemp,2);
       
    ucTemp[0]=1;
    icn85xx_prog_i2c_txdata(cts_dev,0x40644,ucTemp,1);
    while(ucTemp[0]){
        icn85xx_prog_i2c_rxdata(cts_dev,0x40644,ucTemp,1);
    }
    icn85xx_prog_i2c_rxdata(cts_dev,0x40648, (char *)&flashid, 3);
    
    /* Flash ID store in Little Endian, change to CPU endian */
    flashid = le32_to_cpu(flashid) & 0xFFFFFF;
    cts_info("flashid: 0x%06x\n", flashid);

    return flashid;
}

static void FlashWriteEnable(struct cts_device *cts_dev)
{
    unsigned char ucTemp[4] = {0,0,0,0};

    ucTemp[2] = 0x00;
    ucTemp[1] = 0x10;
    ucTemp[0] = 0x00;
    icn85xx_prog_i2c_txdata(cts_dev,0x4062c,ucTemp,3);
    
    ucTemp[0] = 0x06; 
    icn85xx_prog_i2c_txdata(cts_dev,0x40630,ucTemp,1);

    ucTemp[0] = 0x00;
    ucTemp[1] = 0x00;    
    icn85xx_prog_i2c_txdata(cts_dev,0x40640,ucTemp,2);
    
    ucTemp[0] = 1;
    icn85xx_prog_i2c_txdata(cts_dev,0x40644,ucTemp,1);
    while(ucTemp[0]){    
        icn85xx_prog_i2c_rxdata(cts_dev,0x40644,ucTemp,1);
    }
    
    ucTemp[0] = FlashState(cts_dev, 0);
    while( (ucTemp[0]&0x02)!=0x02){
        ucTemp[0] = FlashState(cts_dev, 0);    
    }
}

#ifndef  QUAD_OUTPUT_ENABLE
static void ClearFlashState(struct cts_device *cts_dev )
{
    unsigned char ucTemp[4] = {0,0,0,0};
    icn85xx_prog_i2c_rxdata(cts_dev,0x40603,ucTemp,1);
    ucTemp[0]=(ucTemp[0]|0x20);
    icn85xx_prog_i2c_txdata(cts_dev,0x40603, ucTemp, 1 );
    
    FlashWriteEnable(cts_dev);   
    //write comd to flash
    ucTemp[2]=0x00;
    ucTemp[1]=0x10;
    ucTemp[0]=0x10;
    icn85xx_prog_i2c_txdata(cts_dev,0x4062c,ucTemp,3);
    
    ucTemp[0]=0x01;
    icn85xx_prog_i2c_txdata(cts_dev,0x40630,ucTemp,1);
    
    ucTemp[0]=0x00;
    ucTemp[1]=0x00;
    icn85xx_prog_i2c_txdata(cts_dev,0x40640,ucTemp,2);

    ucTemp[0]=0x00;    
    icn85xx_prog_i2c_txdata(cts_dev,0x40638,ucTemp,1);
    
    ucTemp[0]=1;    
    icn85xx_prog_i2c_txdata(cts_dev,0x40644,ucTemp,1);
    while(ucTemp[0]){
        icn85xx_prog_i2c_rxdata(cts_dev,0x40644,ucTemp,1);
    }
    while(FlashState(cts_dev, 0)&0x01);

}
#else
static void ClearFlashState(void)
{
}
#endif

static void EarseFlash(struct cts_device *cts_dev, 
    unsigned char erase_index, ulong flash_addr)
{
    unsigned char ucTemp[4] = {0,0,0,0};
    FlashWriteEnable(cts_dev);    
    if(erase_index==0){                       //erase the chip
        ucTemp[0]=0xc7;        
        icn85xx_prog_i2c_txdata(cts_dev,0x40630, ucTemp, 1 );
        ucTemp[2]=0x00;
        ucTemp[1]=0x10;
        ucTemp[0]=0x00;     
        icn85xx_prog_i2c_txdata(cts_dev,0x4062c, ucTemp, 3 );  
    }else if(erase_index==1){       //erase 32k space of the flash
        ucTemp[0]=0x52;       
        icn85xx_prog_i2c_txdata(cts_dev,0x40630, ucTemp, 1);
        ucTemp[2]=0x00;
        ucTemp[1]=0x13;
        ucTemp[0]=0x00;        
        icn85xx_prog_i2c_txdata(cts_dev,0x4062c, ucTemp, 3);
    }else if(erase_index==2){     //erase 64k space of the flash
        ucTemp[0]=0xd8;
        icn85xx_prog_i2c_txdata(cts_dev,0x40630, ucTemp,1);
        ucTemp[2]=0x00;
        ucTemp[1]=0x13;
        ucTemp[0]=0x00;
        icn85xx_prog_i2c_txdata(cts_dev,0x4062c, ucTemp, 3);
    }else if(erase_index==3){
        ucTemp[0]=0x20;         
        icn85xx_prog_i2c_txdata(cts_dev,0x40630, ucTemp, 1);
        ucTemp[2]=0x00;
        ucTemp[1]=0x13;
        ucTemp[0]=0x00;       
        icn85xx_prog_i2c_txdata(cts_dev,0x4062c, ucTemp, 3);
    }
    
    ucTemp[2]=(unsigned char)(flash_addr>>16);
    ucTemp[1]=(unsigned char)(flash_addr>>8);
    ucTemp[0]=(unsigned char)(flash_addr);
    icn85xx_prog_i2c_txdata(cts_dev,0x40634, ucTemp, 3);

    ucTemp[1]=0x00;
    ucTemp[0]=0x00;
    icn85xx_prog_i2c_txdata(cts_dev,0x40640, ucTemp, 2 );

    ucTemp[0]=1;
    icn85xx_prog_i2c_txdata(cts_dev,0x40644, ucTemp, 1);
    
    while(ucTemp[0]){
        icn85xx_prog_i2c_rxdata(cts_dev,0x40644,ucTemp,1);
    }      
}

static int  icn85xx_erase_flash(struct cts_device *cts_dev)
{
    int i;
    
    ClearFlashState(cts_dev); 
    while(FlashState(cts_dev,0)&0x01);
    FlashWriteEnable(cts_dev);
    EarseFlash(cts_dev,1,0);            
    while((FlashState(cts_dev,0)&0x01));  

    for(i=0; i<7; i++){
        FlashWriteEnable(cts_dev);
        EarseFlash(cts_dev, 3,0x8000+i*0x1000);            
        while((FlashState(cts_dev, 0)&0x01)); 
    }
    for(i=0; i<4; i++){
        FlashWriteEnable(cts_dev);
        EarseFlash(cts_dev,3,0x10000+i*0x1000);            
        while((FlashState(cts_dev,0)&0x01)); 
    }    
    return 0;
}

static int  icn85xx_prog_buffer(struct cts_device *cts_dev,
    u32 flash_addr,u32 sram_addr,u32 copy_length,u8 program_type)
{
    u8 ucTemp[4] = {0,0,0,0};
    u8 prog_state=0;
    u32 i=0;
    u8 program_commond=0;
    
    if(program_type == 0){
        program_commond = 0x02;
    }else if(program_type == 1){
        program_commond = 0xf2;
    }else{
        program_commond = 0x02;
    }
     
    for(i=0; i<copy_length; ){
        prog_state=(FlashState(cts_dev,0)&0x01);
        while(prog_state){
            prog_state=(FlashState(cts_dev,0)&0x01);
        }
        FlashWriteEnable(cts_dev);           

        ucTemp[2]=0;
        ucTemp[1]=0x13;
        ucTemp[0]=0;
        icn85xx_prog_i2c_txdata(cts_dev,0x4062c, ucTemp, 3);

        ucTemp[2]=(unsigned char)(flash_addr>>16);
        ucTemp[1]=(unsigned char)(flash_addr>>8);
        ucTemp[0]=(unsigned char)(flash_addr);
        icn85xx_prog_i2c_txdata(cts_dev,0x40634, ucTemp, 3);
        
        ucTemp[2]=(unsigned char)(sram_addr>>16);
        ucTemp[1]=(unsigned char)(sram_addr>>8);
        ucTemp[0]=(unsigned char)(sram_addr);
        icn85xx_prog_i2c_txdata(cts_dev,0x4063c, ucTemp, 3);

        if(i+256<=copy_length){
            ucTemp[1]=0x01;
            ucTemp[0]=0x00;
        }else{
            ucTemp[1]=(unsigned char)((copy_length-i)>>8);
            ucTemp[0]=(unsigned char)(copy_length-i);
        }
        icn85xx_prog_i2c_txdata(cts_dev,0x40640, ucTemp,2);

        ucTemp[0]=program_commond;
        icn85xx_prog_i2c_txdata(cts_dev,0x40630, ucTemp,1);

        ucTemp[0]=0x01;
        icn85xx_prog_i2c_txdata(cts_dev,0x40644, ucTemp,1);
        
        flash_addr+=256;
        sram_addr+=256;
        i+=256;
        while(ucTemp[0]){
            icn85xx_prog_i2c_rxdata(cts_dev,0x40644,ucTemp,1);
        }
    }     
  
    prog_state=(FlashState(cts_dev,0)&0x01);
    while(prog_state){
        prog_state=(FlashState(cts_dev, 0)&0x01);
    }
    return 0;   
}

static int  icn85xx_prog_data(struct cts_device *cts_dev, u32 flash_addr, u32 data)
{
    int ret;
    unsigned char ucTemp[4] = {0,0,0,0};
    
    ucTemp[3]=(unsigned char)(data>>24);
    ucTemp[2]=(unsigned char)(data>>16);
    ucTemp[1]=(unsigned char)(data>>8);
    ucTemp[0]=(unsigned char)(data);
    
    ret = icn85xx_prog_i2c_txdata(cts_dev,0x2a000, ucTemp,4);
    if(ret){
        return ret;
    }
    ret = icn85xx_prog_buffer(cts_dev,flash_addr , 0x2a000, 0x04,  0);
    if(ret){
        return ret;
    }

    return 0;
}

static void  icn85xx_read_flash(struct cts_device *cts_dev, u32 sram_address,
    u32 flash_address,unsigned long copy_length,u8 i2c_wire_num)
{
    unsigned char ucTemp[4] = {0,0,0,0};

    if(i2c_wire_num==1){
        ucTemp[2]=0x18;
        ucTemp[1]=0x13;
        ucTemp[0]=0x00;
    }else if(i2c_wire_num==2){
        ucTemp[2]=0x1a;
        ucTemp[1]=0x13;
        ucTemp[0]=0x01;
    }else if(i2c_wire_num==4){
        ucTemp[2]=0x19;
        ucTemp[1]=0x13;
        ucTemp[0]=0x01;
    }else{
        ucTemp[2]=0x18;
        ucTemp[1]=0x13;
        ucTemp[0]=0x01;
    }
    icn85xx_prog_i2c_txdata(cts_dev,0x4062c, ucTemp,3);

    if(i2c_wire_num==1){           
        ucTemp[0]=0x03;
    }else if(i2c_wire_num==2){
        ucTemp[0]=0x3b;
    }else if(i2c_wire_num==4){
        ucTemp[0]=0x6b;
    }else{
        ucTemp[0]=0x0b;
    }   
    icn85xx_prog_i2c_txdata(cts_dev,0x40630, ucTemp,1);
    
    ucTemp[2]=(unsigned char)(flash_address>>16);
    ucTemp[1]=(unsigned char)(flash_address>>8);
    ucTemp[0]=(unsigned char)(flash_address);
    icn85xx_prog_i2c_txdata(cts_dev,0x40634, ucTemp,3);    

    ucTemp[2]=(unsigned char)(sram_address>>16);
    ucTemp[1]=(unsigned char)(sram_address>>8);
    ucTemp[0]=(unsigned char)(sram_address);
    icn85xx_prog_i2c_txdata(cts_dev,0x4063c, ucTemp,3);
    
    ucTemp[1]=(unsigned char)(copy_length>>8);
    ucTemp[0]=(unsigned char)(copy_length);
    icn85xx_prog_i2c_txdata(cts_dev,0x40640, ucTemp,2);
    
    ucTemp[0]=0x01;

    icn85xx_prog_i2c_txdata(cts_dev,0x40644, ucTemp,1);
    while(ucTemp[0]){
        icn85xx_prog_i2c_rxdata(cts_dev,0x40644,ucTemp,1);
    }
}

#if 0
static short  icn85xx_read_fw_Ver(char *fw)
{
    short FWversion;
    char tmp[2];
    int file_size;
    
    file_size = icn85xx_open_fw(fw);
    if(file_size < 0){
        return -1;  
    }   
    icn85xx_read_fw(0x100, 2, &tmp[0]);
    FWversion = (tmp[1]<<8)|tmp[0];
    //cts_info("FWversion: 0x%x\n", FWversion);
    return FWversion;
}
#endif

static int icn87xx_read_fw_info(char *fw, unsigned char *buffer, unsigned short u16Addr, unsigned char u8Length)
{
    int file_size;
    file_size = icn85xx_open_fw(fw);
    if(file_size < 0){
        return -1;  
    }   
    icn85xx_read_fw(u16Addr, u8Length, buffer);
    return 1;
}

static int  icn85xx_fw_download(struct cts_device *cts_dev, u32 offset, u8 * buffer, u32 size)
{
    int ret;
#ifdef ENABLE_BYTE_CHECK  
    int i = 0;    
    char testb[B_SIZE];
#endif

    ret = icn85xx_prog_i2c_txdata(cts_dev,offset,buffer,size);
    if(ret){
        return ret;
    }
#ifdef ENABLE_BYTE_CHECK  
    ret = icn85xx_prog_i2c_rxdata(cts_dev,offset,testb,size);   
    if(ret){
        return ret;
    }

    for(i = 0; i < size; i++){
        if(buffer[i] != testb[i]){
            cts_err("buffer[%d]:%x  testb[%d]:%x\n",i,buffer[i],i,testb[i]);
            return -1;
        }  
    }
#endif 
    return 0;   
}

#if 0
static int  icn85xx_bootfrom_flash(struct cts_device *cts_dev, int ictype)
{
    int ret = -1;
    unsigned char ucTemp[3];
    
    cts_info("icn85xx_bootfrom_flash: 0x%x", ictype);

    icn85xx_prog_i2c_rxdata(cts_dev,0x40000,ucTemp,3);
    cts_info("ucTemp: 0x%x 0x%x\n", ucTemp[1], ucTemp[2]);
    
    if(cts_dev->hwdata->is_with_flash){     //(ictype == ICN85XX_WITH_FLASH_85)
        ucTemp[0]=0x7f;            
        ret = icn85xx_prog_i2c_txdata(cts_dev,0x40004, &ucTemp[0], 1 );//ICN85XX chip reset 
        if (ret) {
            cts_err("111 %s failed: %d\n", __func__, ret); 
            return ret;
        }
    }else if(cts_dev->hwdata->is_with_flash){       //(ictype == ICN85XX_WITH_FLASH_86)
        ucTemp[0]=0x7f;            
        ret = icn85xx_prog_i2c_txdata(cts_dev,0x4046c, &ucTemp[0], 1 );//ICN85EX chip reset 
        if (ret) {
            cts_err("333 %s failed: %d\n", __func__, ret); 
            return ret;
        }  
    }
    return ret;
}
#endif

int  icn85xx_bootfrom_sram(struct cts_device *cts_dev)
{
    int ret = -1;   
    unsigned char ucTemp = 0x03;                      
    unsigned long addr = 0x40400; 
    
    cts_info("icn85xx_bootfrom_sram\n");
    ret = icn85xx_prog_i2c_txdata(cts_dev,addr, &ucTemp, 1 );           //change bootmode from sram   
    return ret;
}
 
//This polynomial (0x04c11db7) is used at: AUTODIN II, Ethernet, & FDDI 
static unsigned int crc32table[256] = {    
     0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,    
     0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,    
     0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,    
     0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,    
     0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,    
     0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,    
     0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,    
     0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,    
     0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,    
     0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,    
     0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,    
     0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,    
     0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,    
     0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,    
     0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,    
     0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,    
     0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,    
     0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,    
     0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,    
     0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,    
     0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,    
     0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,    
     0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,    
     0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,    
     0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,    
     0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,    
     0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,    
     0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,    
     0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,    
     0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,    
     0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,    
     0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,    
     0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,    
     0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,    
     0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,    
     0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,    
     0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,    
     0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,    
     0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,    
     0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,    
     0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,    
     0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,    
     0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static unsigned int icn85xx_crc_calc(unsigned crc_in, char *buf, int len)  
{
    int i;    
    unsigned int crc = crc_in;        
    for(i = 0; i < len; i++){
        crc = (crc << 8) ^ crc32table[((crc >> 24) ^ *buf++) & 0xFF];   
    }
    return crc;
}

static int  icn85xx_crc_enable(struct cts_device *cts_dev, unsigned char enable)
{
    unsigned char ucTemp;
    int ret = 0;
    if(enable==1){
        ucTemp = 1;
        ret = icn85xx_prog_i2c_txdata(cts_dev,0x40028, &ucTemp, 1 );
        if(ret){
            return ret;
        }
    }else if(enable==0){
        ucTemp = 0;
        ret = icn85xx_prog_i2c_txdata(cts_dev,0x40028, &ucTemp, 1 );
        if(ret){
            return ret;
        }
    } 
    return ret;
}

static int  icn85xx_crc_check(struct cts_device *cts_dev, u32 crc, u32 len)
{
    int ret;
    unsigned int crc_len;
    unsigned int crc_result;    
    unsigned char ucTemp[4] = {0,0,0,0};     
    
    ret= icn85xx_prog_i2c_rxdata(cts_dev,0x4002c, ucTemp, 4 );
    if(ret){
         return ret;
    }    
    crc_result = ucTemp[3]<<24 | ucTemp[2]<<16 | ucTemp[1] << 8 | ucTemp[0];
    //cts_info("crc_result: 0x%x\n", crc_result);
    
    ret = icn85xx_prog_i2c_rxdata(cts_dev,0x40034, ucTemp, 2);
    if(ret){
         return ret;
    }    
    crc_len = ucTemp[1] << 8 | ucTemp[0];
    //cts_info("crc_len: %d\n", crc_len);

    if((crc_result == crc) && (crc_len == len)){
        return 0;
    }else{
        cts_info("crc_fw: 0x%x\n", crc);
        cts_info("crc_result: 0x%x\n", crc_result);
        cts_info("crc_len: %d\n", crc_len);
        return -1;
    }
}

int  icnt85xx_fw_update(struct cts_device *cts_dev,
    const struct cts_firmware *firmware, bool to_flash)
{
    int file_size, last_length;
    int j, num;
    unsigned char temp_buf[B_SIZE];
    unsigned int crc_fw;

    cts_info("Into icnt85xx or 86xx or 88xx_fw_update...");

    icn85xx_set_fw(firmware->data, firmware->size);	

    file_size = icn85xx_open_fw(firmware->data);
    if(file_size < 0){
        icn85xx_update_status(R_FILE_ERR);
        return R_FILE_ERR;  
    }
    
#if 0
    if(icn85xx_goto_progmode(cts_dev) != 0)
    {
        icn85xx_update_status(R_PROGRAM_ERR);
        cts_err("icn85xx_goto_progmode() != 0 error");
        return R_STATE_ERR; 
    } 
    msleep(1);
#endif

    icn85xx_crc_enable(cts_dev, 1);
    cts_info("icn85xx_fw_download start");
    num = file_size/B_SIZE;   
    crc_fw = 0;
    for(j=0; j < num; j++){
        icn85xx_read_fw(j*B_SIZE, B_SIZE, temp_buf);        
        crc_fw = icn85xx_crc_calc(crc_fw, temp_buf, B_SIZE);
        if(icn85xx_fw_download(cts_dev, j*B_SIZE, temp_buf, B_SIZE) != 0){
            cts_err("error j:%d",j);
            icn85xx_update_status(R_PROGRAM_ERR);
            return R_PROGRAM_ERR;
        }    
        icn85xx_update_status(5+(int)(60*j/num));
    }
    last_length = file_size - B_SIZE*j;
    if(last_length > 0){
        icn85xx_read_fw( j*B_SIZE, last_length, temp_buf);     
        crc_fw = icn85xx_crc_calc(crc_fw, temp_buf, last_length);
        if(icn85xx_fw_download(cts_dev, j*B_SIZE, temp_buf, last_length) != 0){
            cts_err("error last length");
            icn85xx_update_status(R_PROGRAM_ERR);
            return R_PROGRAM_ERR;
        }
    }    
    icn85xx_update_status(65);
    //cts_info("crc_fw: 0x%x\n", crc_fw);
    //msleep(1);
    icn85xx_crc_enable(cts_dev, 0);
    if(icn85xx_crc_check(cts_dev, crc_fw, file_size) != 0){
        icn85xx_update_status(R_VERIFY_ERR);
        cts_info("down fw error, crc error");
        return R_PROGRAM_ERR;
    }else{
        //cts_info("downoad fw ok, crc ok\n");
    }
    icn85xx_update_status(70);
    cts_info("icn85xx_fw_download ok"); 

    if(!to_flash){
    #if 0
        if(icn85xx_bootfrom_sram(cts_dev) != 0){
            cts_err("icn85xx_bootfrom_sram error\n");
            icn85xx_update_status(R_STATE_ERR);
            return R_STATE_ERR;
        }
    #endif
        cts_info("firmware download to SRAM OK!");
        return R_OK;
    }
   
    if(cts_dev->hwdata->is_with_flash){
        icn85xx_erase_flash(cts_dev);
        icn85xx_update_status(75);
        FlashWriteEnable(cts_dev);
        icn85xx_prog_buffer(cts_dev, 0, 0, file_size,0);
        icn85xx_update_status(85);
        while((FlashState(cts_dev,0)&0x01)); 
        
        FlashWriteEnable(cts_dev);
        icn85xx_prog_data(cts_dev, FLASH_CRC_ADDR, crc_fw);
        icn85xx_prog_data(cts_dev, FLASH_CRC_ADDR+4, file_size);
        icn85xx_update_status(90);
        icn85xx_crc_enable(cts_dev, 1);   
        icn85xx_read_flash(cts_dev, 0,  0, file_size,  2);    
        icn85xx_crc_enable(cts_dev, 0);
        if(icn85xx_crc_check(cts_dev, crc_fw, file_size) != 0){
            cts_info("read flash data error, crc error");
            return R_PROGRAM_ERR;
        }else{
            cts_info("read flash data ok, crc ok");
        }                  
        while((FlashState(cts_dev, 0)&0x01));
        icn85xx_update_status(95);
        
        cts_info("firmware download to flash OK!");
        #if 0
        if(icn85xx_bootfrom_flash() == 0)
        if(icn85xx_bootfrom_sram(cts_dev) != 0)    //code already in ram
        {
            cts_err("icn85xx_bootfrom_flash error\n");
             icn85xx_update_status(R_STATE_ERR);
             return R_STATE_ERR;
         }
        #endif
    }else{//ICN85XX_WITHOUT_FLASH_85
         #if 0   
         if(icn85xx_bootfrom_sram(cts_dev) != 0)
         {
             cts_err("icn85xx_bootfrom_sram error\n");
             icn85xx_update_status(R_STATE_ERR);
             return R_STATE_ERR;
         }
         #endif
        cts_info("chip without flash firmware download to SRAM OK!");
    }
    msleep(50);
    icn85xx_update_status(R_OK);
    cts_info("icn85xx upgrade ok");
    return R_OK;
}

//-----------------for icnt81xx and icn87xx fw update-----------------/

int icn87xx_boot_sram(struct cts_device *cts_dev)
{
    int ret = -1;
    unsigned char ucTemp = 0x03;
    ret = icn87xx_prog_i2c_txdata(cts_dev, 0xf400,&ucTemp,1);
    return ret;
}

#if 0
static int icn87xx_boot_flash(struct cts_device *cts_dev)
{
   //need pull low int here
    int ret = -1;
    unsigned char ucTemp = 0x7f;
    ret = icn87xx_prog_i2c_txdata(cts_dev, 0xf008,&ucTemp,1);//chip rest
    return ret;
}
#endif

static int icn87xx_read_flashstate(struct cts_device *cts_dev)
{
    int ret = -1;
    unsigned char ucTemp[2] = {0 , 0};
    
    ucTemp[0] = 1;
    while(ucTemp[0]){
        ret = icn87xx_prog_i2c_rxdata(cts_dev, SF_BUSY_87,&ucTemp[0],1);
        if(ret){
            return ret;
        }
    }
    ucTemp[0] = FLASH_CMD_READ_STATUS;
    ret = icn87xx_prog_i2c_txdata(cts_dev, CMD_SEL_87, &ucTemp[0],1);
    if(ret){
        return ret;
    }

    ucTemp[1] = (unsigned char)(SRAM_EXCHANGE_ADDR>>8);
    ucTemp[0] = (unsigned char)(SRAM_EXCHANGE_ADDR);
    ret = icn87xx_prog_i2c_txdata(cts_dev, SRAM_ADDR_87, &ucTemp[0],2);
    if(ret){
        return ret;
    }
    ucTemp[0] = 1;
    ret = icn87xx_prog_i2c_txdata(cts_dev, START_DEXC_87, &ucTemp[0],1);
    if(ret){
        return ret;
    }

    while(ucTemp[0]){
        ret = icn87xx_prog_i2c_rxdata(cts_dev, SF_BUSY_87,&ucTemp[0],1);
        if(ret){
            return ret;
        }
    }
    ret = icn87xx_prog_i2c_rxdata(cts_dev, SRAM_EXCHANGE_ADDR,&ucTemp[0],1);
    if(ret){
        return ret;
    }
    return ucTemp[0];
}

int  icn87xx_read_flashid(struct cts_device *cts_dev)
{
    int ret = -1;
    int flash_id;
    unsigned char ucTemp[4];

    ucTemp[0] = FLASH_CMD_READ_IDENTIFICATION;
    ret = icn87xx_prog_i2c_txdata(cts_dev, CMD_SEL_87, ucTemp,1);
    if(ret){
        return ret;
    }
    ucTemp[1] = (unsigned char)(SRAM_EXCHANGE_ADDR1 >> 8);
    ucTemp[0] = (unsigned char)SRAM_EXCHANGE_ADDR1;
    ret = icn87xx_prog_i2c_txdata(cts_dev, SRAM_ADDR_87, ucTemp,2);
    if(ret){
        return ret; 
    }
    ucTemp[0] = 1;
    ret = icn87xx_prog_i2c_txdata(cts_dev, START_DEXC_87, ucTemp,1);
    if(ret){
        return ret;
    }

    while(ucTemp[0]){
        ret = icn87xx_prog_i2c_rxdata(cts_dev, SF_BUSY_87,&ucTemp[0],1);
        if(ret){
            return ret;
        }
    }
    ret = icn87xx_prog_i2c_rxdata(cts_dev, SRAM_EXCHANGE_ADDR1, (char *)&flash_id, 3);
    if(ret){
        return ret;
    }
    /* Flash ID store in Big Endian, change to CPU endian */
    flash_id = (be32_to_cpu(flash_id)>>8) & 0xFFFFFF;
    cts_info("flashid: 0x%06x\n", flash_id);

    return flash_id;
}

int icn87xx_calculate_crc(struct cts_device *cts_dev, unsigned short len)
{
    unsigned char ucTemp[4];
    int ret = -1;
    int crc = 0;

    //2.1 set address
    ucTemp[0] = 0x00;
    ucTemp[1] = 0x00;
    ret = icn87xx_prog_i2c_txdata(cts_dev, SRAM_ADDR_87, ucTemp,2);
    if(ret){
        return ret;
    }
    //2.2 set length
    ucTemp[0] = len & 0xff;
    ucTemp[1] = (len >> 8 )& 0xff;
    ret = icn87xx_prog_i2c_txdata(cts_dev, DATA_LENGTH_87, ucTemp,2);
    if(ret){
        return ret;
    }
    //2.3 start calculate
    ucTemp[0] = 0x01;
    ret = icn87xx_prog_i2c_txdata(cts_dev, SW_CRC_START_87, ucTemp,1);
    if(ret){
        return ret;
    }

    //2.4 poll status
    while(ucTemp[0]){
        ret = icn87xx_prog_i2c_rxdata(cts_dev, SF_BUSY_87, ucTemp,1);
        if(ret < 0){
            return ret;
        }
    }
    ret = icn87xx_prog_i2c_rxdata(cts_dev, CRC_RESULT_87, ucTemp,2);
    if(ret < 0){
        return ret;
    }
    crc = (((ucTemp[1])<<8) + (ucTemp[0]))&0x0000ffff;

    return crc;
}

static unsigned short icn87xx_fw_Crc(unsigned short crc,unsigned char *buf ,unsigned short size)
{
    u16 u16CrcValue = crc;
    u16 u16Length = size;
    u8  u8CheckData = 0;
    
    while(u16Length){
        u8CheckData = *buf++;
        u16CrcValue = (u16CrcValue << 8)^Crc16Table[(u16CrcValue >> 8)^((u8CheckData)&0xff)];
        u16Length--;
        //buf++;
    }
    return u16CrcValue;
}

static int icn87xx_erase_flash(struct cts_device *cts_dev, u8 u8EraseMode, u32 u32FlashAddr)
{
    u8 ucTemp[4];
    int ret = -1;   
    
    if(FLASH_EARSE_4K == u8EraseMode){
        ucTemp[0] = FLASH_CMD_ERASE_SECTOR;
    }else if(FLASH_EARSE_32K == u8EraseMode){
        ucTemp[0] = FLASH_CMD_ERASE_BLOCK;
    }
    ret = icn87xx_prog_i2c_txdata(cts_dev, CMD_SEL_87, ucTemp,1);
    if(ret){
        return ret;
    }

    ucTemp[2] =(u8)(u32FlashAddr >> 16);
    ucTemp[1] =(u8)(u32FlashAddr >> 8);
    ucTemp[0] =(u8)(u32FlashAddr );
    ret = icn87xx_prog_i2c_txdata(cts_dev, FLASH_ADDR_87, ucTemp,3);
    if(ret){
        return ret;
    }
    ucTemp[0] = 1;
    ret = icn87xx_prog_i2c_txdata(cts_dev, START_DEXC_87, ucTemp,1);
    if(ret){
        return ret;
    }

    while(ucTemp[0]){
        ret = icn87xx_read_flashstate(cts_dev);
        if(ret<0){
            return ret;
        }
        ucTemp[0] = (unsigned char)(ret&0x01);
    }

    return 0;

}

static int icn87xx_erase_chip(struct cts_device *cts_dev)
{
    int ret = -1;
    
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_4K, 0xe000);
    if(ret){
        return ret;
    }
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_32K, 0);
    if(ret){
        return ret;
    }
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_4K, 0x8000);
    if(ret){
        return ret;
    }
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_4K, 0x9000);
    if(ret){
        return ret;
    }
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_4K, 0xa000);
    if(ret){
        return ret;
    }
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_4K, 0xb000);
    if(ret){
        return ret;
    }
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_4K, 0xc000);
    if(ret){
        return ret;
    }
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_4K, 0xd000);
    if(ret){
        return ret;
    }
    ret = icn87xx_erase_flash(cts_dev, FLASH_EARSE_32K, 0x10000);
    if(ret){
        return ret;
    }
    
    return ret;
}

static int icn87xx_flash_write(struct cts_device *cts_dev, u32 u32FlashAddr, 
        u32 u32SramAddr, u32 u32Length)
{
    u8 ucTemp[3] = {0, 0, 0};
    int ret = -1;
    u32 u32FlashTempAddr = u32FlashAddr;
    u32 u32SramTempAddr = u32SramAddr;
    u16 u16NotAlignLength;

    u16 i = 0;
    for(i = 0; i < u32Length;){                         //should not i++
        ucTemp[0] = 1;                                   //confirm the flash whether in busy state 
        while(ucTemp[0]){
            ret = icn87xx_read_flashstate(cts_dev);
            if(ret < 0){
                return ret;
            }
            ucTemp[0] = (unsigned char)(ret&0x01);
        }
        
        ucTemp[2] = (u8)(u32FlashTempAddr >> 16);
        ucTemp[1] = (u8)(u32FlashTempAddr >> 8);
        ucTemp[0] = (u8)(u32FlashTempAddr );   
        ret = icn87xx_prog_i2c_txdata(cts_dev, FLASH_ADDR_87, ucTemp,3);
        if(ret){
            return ret;
        }
        
        ucTemp[2] = (u8)(u32SramTempAddr >> 16);
        ucTemp[1] = (u8)(u32SramTempAddr >> 8);
        ucTemp[0] = (u8)(u32SramTempAddr );  
        ret = icn87xx_prog_i2c_txdata(cts_dev, SRAM_ADDR_87, ucTemp,3);
        if(ret){
            return ret;
        }
        //IO_WRITE32(SRAM_ADDR_87, u32SramTempAddr);    //config sram  addr  
        // check u32FlashTempAddr page aglin
        if(u32FlashTempAddr % 0x100){  // not aglin
            u16NotAlignLength = 0x100 - (u32FlashTempAddr % 0x100);
            if(u32Length <= u16NotAlignLength){
                ucTemp[1] = (u8)(u32Length >> 8);
                ucTemp[0] = (u8)(u32Length );  
                ret = icn87xx_prog_i2c_txdata(cts_dev, DATA_LENGTH_87, ucTemp,2);
                if(ret){
                    return ret;
                }
            }else{
                ucTemp[1] = (u8)(u16NotAlignLength >> 8);
                ucTemp[0] = (u8)(u16NotAlignLength );   
                ret = icn87xx_prog_i2c_txdata(cts_dev, DATA_LENGTH_87, ucTemp,2);
                if(ret){
                    return ret;
                }
            }
            u32FlashTempAddr += u16NotAlignLength;                       //change the flash and sram address
            u32SramTempAddr += u16NotAlignLength;
            i += u16NotAlignLength;
        }else{
            if(i+256<=u32Length){
                ucTemp[1] = (u8)(0x01);
                ucTemp[0] = (u8)(0x00 ); 
                ret = icn87xx_prog_i2c_txdata(cts_dev, DATA_LENGTH_87, ucTemp,2);
                if(ret){
                    return ret;
                }
            }else{
                ucTemp[1] = (u8)((u32Length-i) >> 8);
                ucTemp[0] = (u8)(u32Length-i );   
                ret = icn87xx_prog_i2c_txdata(cts_dev, DATA_LENGTH_87, ucTemp,2);
                if(ret){
                    return ret;
                }
            }
            u32FlashTempAddr += 256;                       //change the flash and sram address
            u32SramTempAddr += 256;
            i += 256;
        }
        ucTemp[0] = FLASH_CMD_PAGE_PROGRAM;    
        ret = icn87xx_prog_i2c_txdata(cts_dev, CMD_SEL_87, ucTemp,1);
        if(ret){
            return ret;
        }
        
        ucTemp[0] = 1;  
        ret = icn87xx_prog_i2c_txdata(cts_dev, START_DEXC_87, ucTemp,1);
        if(ret){
            return ret;
        }
    }
     
    ucTemp[0] = 1;    
    while(ucTemp[0]){                                 //confirm whether the last pageprogram whether complete
        ret = icn87xx_read_flashstate(cts_dev);
        if(ret<0){
            return ret;
        }
        ucTemp[0] = (unsigned char)(ret&0x01);
    }
    return ret;
} 

#if 0
static int icn87xx_flash_read(struct cts_device *cts_dev, u32 u32SramAddr, 
    u32 u32FlashAddr, u16 u16Length)
{
    u8 ucTemp[4] = {0, 0, 0, 0};
    int ret = -1;
    
    ucTemp[0] = FLASH_CMD_FAST_READ;
    ret = icn87xx_prog_i2c_txdata(cts_dev, CMD_SEL_87, ucTemp,1);
    if(ret < 0){
        return ret;
    }
    ucTemp[3] = (u8)(u32FlashAddr >> 24);
    ucTemp[2] = (u8)(u32FlashAddr >> 16);
    ucTemp[1] = (u8)(u32FlashAddr >> 8);
    ucTemp[0] = (u8)(u32FlashAddr);
    ret = icn87xx_prog_i2c_txdata(cts_dev, FLASH_ADDR_87, ucTemp,3);
    if(ret){
        return ret;
    }
    
    ucTemp[3] = (u8)(u32SramAddr >> 24);
    ucTemp[2] = (u8)(u32SramAddr >> 16);
    ucTemp[1] = (u8)(u32SramAddr >> 8);
    ucTemp[0] = (u8)(u32SramAddr);
    ret = icn87xx_prog_i2c_txdata(cts_dev, SRAM_ADDR_87, ucTemp,3);
    if(ret){
        return ret;
    }
    ucTemp[1] = (u8)(u16Length >> 8);
    ucTemp[0] = (u8)(u16Length);
    ret = icn87xx_prog_i2c_txdata(cts_dev, DATA_LENGTH_87, ucTemp,2);
    if(ret){
        return ret;
    }
    ucTemp[0] = 1;
    ret = icn87xx_prog_i2c_txdata(cts_dev, START_DEXC_87, ucTemp,1);
    if(ret){
        return ret;
    }

    while(ucTemp[0]){
        ret = icn87xx_read_flashstate(cts_dev);
        if(ret<0){
            return ret;
        }
        ucTemp[0] = (unsigned char)(ret&0x01);
    }
    return ret;
}
#endif

static int icn87xx_write_flash_info(struct cts_device *cts_dev, u32 u32FlashAddr, 
    u32 u32SramAddr, u8 *buf, u16 u16Length)
{
    int ret = -1;
    ret = icn87xx_prog_i2c_txdata(cts_dev, u32SramAddr, buf, u16Length);
    if(ret){
        return ret;
    }
    ret = icn87xx_flash_write(cts_dev, u32FlashAddr, u32SramAddr, u16Length);
    return ret;
}



static int  icn87xx_fw_download(struct cts_device *cts_dev, unsigned int offset, unsigned char * buffer, unsigned int size)
{
#ifdef ENABLE_BYTE_CHECK      
    char testb[B_SIZE];
#endif

    icn87xx_prog_i2c_txdata(cts_dev, offset,buffer,size);   
#ifdef ENABLE_BYTE_CHECK  
    icn87xx_prog_i2c_rxdata(cts_dev, offset,testb,size);   
    for(i = 0; i < size; i++){
        if(buffer[i] != testb[i]){
            cts_err("buffer[%d]:%x  testb[%d]:%x\n",i,buffer[i],i,testb[i]);
            return -1;
        }  
    }
#endif 
    return 0;   
}

int  icnt87xx_fw_update(struct cts_device *cts_dev,
    const struct cts_firmware *firmware, bool to_flash)
{
    int file_size, last_length;
    //char version = 0;
    int ret = -1;
    int j, num;
    unsigned char temp_buf[B_SIZE];
    unsigned short crc_fw = 0;
    unsigned short sram_len = 0; 
    unsigned short sram_crc = 0;
    unsigned short temp_crc = 0;
    unsigned short fw_version = 0;
    //unsigned int flash_addr = 0;

    cts_info("Into icnt81xx or 87xx_fw_update...");

    icn85xx_set_fw(firmware->data, firmware->size);

    ////temp_buf[0]-[3] data_start_addr,[4]-[7]:data_end_addr,[8]-[11],text_len,[12]-[15]:fwversion
    ret = icn87xx_read_fw_info(firmware->data, temp_buf, FIRMWARA_INFO_AT_BIN_ADDR,16); //contain sram lenth ,fwversion
    sram_len = (((temp_buf[9])<<8) + temp_buf[8])
        + (((temp_buf[6])<<16) + ((temp_buf[5])<<8) + temp_buf[4])
        - (((temp_buf[2])<<16) + ((temp_buf[1])<<8) + temp_buf[0]);
    fw_version = (((temp_buf[13])<<8)+ temp_buf[12]);
    
    file_size = icn85xx_open_fw(firmware->data);
    
    cts_info("file_size:%d",file_size);
    //printk("boot mode:0x%x\n",boot_mode);
    if(file_size < 0){
        icn85xx_update_status(R_FILE_ERR);
        return R_FILE_ERR;  
    }
#if 1
    if(icn85xx_goto_progmode(cts_dev) < 0)
    {
        icn85xx_update_status(R_PROGRAM_ERR);
        cts_err("icn85xx_goto_progmode()  error\n");
        return R_STATE_ERR; 
    } 
#endif

    cts_info("sram_len:%d",sram_len);
    msleep(1); 
	 cts_info("cts_dev->hwdata->is_with_flash:%d",cts_dev->hwdata->is_with_flash);
    if(cts_dev->hwdata->is_with_flash){//(ICN85XX_WITH_FLASH_87 == boot_mode)
         ret = icn87xx_erase_chip(cts_dev);
         if(ret){
              cts_err("earse error\n");
              return ret;
         }      
    }
     
    if(sram_len == file_size){
        num = file_size/B_SIZE;   
        crc_fw = 0;
        for(j=0; j < num; j++){
            icn85xx_read_fw(j*B_SIZE, B_SIZE, temp_buf);        
            crc_fw = icn87xx_fw_Crc(crc_fw, temp_buf, B_SIZE);
            if(icn87xx_fw_download(cts_dev, j*B_SIZE, temp_buf, B_SIZE) != 0){
                cts_err("error j:%d\n",j);
                icn85xx_update_status(R_PROGRAM_ERR);
                return R_PROGRAM_ERR;
            }    
            icn85xx_update_status(5+(int)(60*j/num));
        }
        
        last_length = file_size - B_SIZE*num;
        if(last_length > 0){
            icn85xx_read_fw(j*B_SIZE, last_length, temp_buf);     
            crc_fw = icn87xx_fw_Crc(crc_fw, temp_buf, last_length);
            if(icn87xx_fw_download(cts_dev, j*B_SIZE, temp_buf, last_length) != 0){
                cts_err("error last length\n");
                icn85xx_update_status(R_PROGRAM_ERR);
                return R_PROGRAM_ERR;
            }
        }  
        
        if( icn87xx_calculate_crc(cts_dev, sram_len) != crc_fw){
            cts_err("crc error\n");
            return -1;
        }else{
            sram_crc =  crc_fw;
        }
        
        if(cts_dev->hwdata->is_with_flash){//(ICN85XX_WITH_FLASH_87 == boot_mode)
            ret = icn87xx_flash_write(cts_dev,0, 0, file_size);
            if(ret){
                return ret;
            }
        }
    }else if(file_size > sram_len){
        if(!cts_dev->hwdata->is_with_flash){//(ICN85XX_WITHOUT_FLASH_87 == boot_mode)
            cts_err("error!! file_size > sram_len\n");
            icn85xx_update_status(R_PROGRAM_ERR);
            return R_PROGRAM_ERR;
        }
        num = sram_len/B_SIZE;   
        crc_fw = 0;
        for(j = 0; j < num; j++){
            icn85xx_read_fw(j*B_SIZE, B_SIZE, temp_buf);        
            crc_fw = icn87xx_fw_Crc(crc_fw, temp_buf, B_SIZE);
            if(icn87xx_fw_download(cts_dev,j*B_SIZE, temp_buf, B_SIZE) != 0){
                cts_err("error j:%d\n",j);
                icn85xx_update_status(R_PROGRAM_ERR);
                return R_PROGRAM_ERR;
            }    
            icn85xx_update_status(5+(int)(60*j/num));
        }
        
        last_length = sram_len - B_SIZE*num;
        if(last_length > 0){
            icn85xx_read_fw(j*B_SIZE, last_length, temp_buf);     
            crc_fw = icn87xx_fw_Crc(crc_fw, temp_buf, last_length);
            if(icn87xx_fw_download(cts_dev, j*B_SIZE, temp_buf, last_length) != 0){
                cts_err("error last length\n");
                icn85xx_update_status(R_PROGRAM_ERR);
                return R_PROGRAM_ERR;
            }
        }   
    
        if(icn87xx_calculate_crc(cts_dev, sram_len) != crc_fw){
            cts_err("crc error0\n");
            return -1;
        }else{
            sram_crc =  crc_fw;
        }
        
        ret = icn87xx_flash_write(cts_dev, 0, 0, sram_len);        
        if(ret){
            return ret;
        }

        num = (file_size - sram_len)/B_SIZE;  
        temp_crc = 0;
        
        for(j=0; j < num; j++){
            icn85xx_read_fw(sram_len + j*B_SIZE, B_SIZE, temp_buf);        
            crc_fw = icn87xx_fw_Crc(crc_fw, temp_buf, B_SIZE);
            temp_crc = icn87xx_fw_Crc(temp_crc, temp_buf, B_SIZE);
            if(icn87xx_fw_download(cts_dev, j*B_SIZE, temp_buf, B_SIZE) != 0) {
                cts_err("error j:%d\n",j);
                icn85xx_update_status(R_PROGRAM_ERR);
                return R_PROGRAM_ERR;
            }    
            icn85xx_update_status(5+(int)(60*j/num));
        }
        
        last_length = (file_size - sram_len) - B_SIZE*num;
        if(last_length > 0){
            icn85xx_read_fw( sram_len + j*B_SIZE, last_length, temp_buf);     
            crc_fw = icn87xx_fw_Crc(crc_fw, temp_buf, last_length);
            temp_crc = icn87xx_fw_Crc(temp_crc, temp_buf, last_length);
            if(icn87xx_fw_download(cts_dev, j*B_SIZE, temp_buf, last_length) != 0){
                cts_err("error last length\n");
                icn85xx_update_status(R_PROGRAM_ERR);
                return R_PROGRAM_ERR;
            }
        }   
        if(icn87xx_calculate_crc(cts_dev, file_size - sram_len) != temp_crc){
            cts_err("crc error1\n");
            return -1;
        }
        ret = icn87xx_flash_write(cts_dev,sram_len, 0, file_size - sram_len);
        if(ret){
            return ret;
        }
    }
    
    if(cts_dev->hwdata->is_with_flash){//(ICN85XX_WITH_FLASH_87 == boot_mode)
        cts_info("write flashinfo ");
        icn85xx_update_status(85);
        //////write flash info to flash
        temp_buf[0] = (unsigned char)(crc_fw);
        temp_buf[1] = (unsigned char)(crc_fw>>8);
        temp_buf[2] = 0;
        temp_buf[3] = 0;
        temp_buf[4] = (unsigned char)(file_size);
        temp_buf[5] = (unsigned char)(file_size>>8);
        temp_buf[6] = 0;
        temp_buf[7] = 0;
        temp_buf[8] = 0x5a;
        temp_buf[9] = 0xc4;
        temp_buf[10] = 0;
        temp_buf[11] = 0;
        temp_buf[12] = (unsigned char)(sram_crc);
        temp_buf[13] = (unsigned char)(sram_crc>>8);
        temp_buf[14] = 0;
        temp_buf[15] = 0;
        temp_buf[16] = (unsigned char)(sram_len);
        temp_buf[17] = (unsigned char)(sram_len>>8);
        temp_buf[18] = 0;
        temp_buf[19] = 0;

        ret = icn87xx_write_flash_info(cts_dev,FLASH_STOR_INFO_ADDR,SRAM_EXCHANGE_ADDR1,temp_buf,20);
    #if 0    
        icn85xx_update_status(95);
        msleep(5);
        while(version != 0x81){
            if(icn87xx_boot_flash(cts_dev) != 0){    //code already in ram
                cts_err("icn85xx_bootfrom_flash error");
                icn85xx_update_status(R_STATE_ERR);
                return R_STATE_ERR;
            }
            msleep(10);
            //ret = icn85xx_read_reg(0xa, &version);
            ret = icn87xx_prog_i2c_rxdata(cts_dev, 0x000a, &version,1);
            cts_info("after upgrdate Value: %x",version);
            msleep(5);
        }
    #endif
    }else if(!cts_dev->hwdata->is_with_flash){//(ICN85XX_WITHOUT_FLASH_87 == boot_mode)
    #if 0
        if(icn87xx_boot_sram(cts_dev) != 0){
            cts_err("icn85xx_bootfrom_sram error\n");
            icn85xx_update_status(R_STATE_ERR);
            return R_STATE_ERR;
        }
    #endif
    }
    icn85xx_update_status(R_OK);
    cts_info("icn87xx upgrade ok\n");
    // junfuzhang 20160913, check crc in sram before boot
    icnt87_sram_crc = sram_crc;
    icnt87_sram_length = sram_len;
    //junfuzhang 20160913, adding end
    return R_OK;
}

#if 0
//-----------------------for ICNT89xx---------------------------//
static int icn89xx_goto_progmode(struct cts_device *cts_dev)
{
    int ret = -1;
    unsigned char ucTemp;  

    cts_info("icn89xx_goto_progmode\n"); 

    ucTemp = 0x5a;               
    ret = icn85xx_prog_i2c_txdata(cts_dev,0xcc3355, &ucTemp,1);             
    if(ret){    
         cts_info("icn89xx_goto_progmode error\n");
        mdelay(2); 
    }
    ucTemp = 0x02;               
    ret = icn85xx_prog_i2c_txdata(cts_dev,0x40010, &ucTemp,1);             
    if(ret){    
        cts_info("icn89xx_goto_progmode error\n");
        mdelay(2); 
    }

    ucTemp = 0x5a;               
    ret = icn85xx_prog_i2c_txdata(cts_dev,0xcc3355, &ucTemp,1);             
    if(ret){    
        cts_info("icn89xx_goto_progmode error\n");
        mdelay(2); 
    }
    
    ucTemp = 0x01;               
    ret = icn85xx_prog_i2c_txdata(cts_dev,0x40400, &ucTemp,1);             
    if(ret){    
         cts_info("icn89xx_goto_progmode error\n");
        mdelay(2); 
    }
    
    ucTemp = 0x09;               
    ret = icn85xx_prog_i2c_txdata(cts_dev,0x40158, &ucTemp,1);             
    if(ret){    
        cts_info("icn89xx_goto_progmode error\n");
        mdelay(2); 
    }
    
    ucTemp = 0x00;               
    ret = icn85xx_prog_i2c_txdata(cts_dev,0x4040c, &ucTemp,1);             
    if(ret){    
        cts_info("icn89xx_goto_progmode error\n");
        mdelay(2); 
    }
    
    ucTemp = 0x00;               
    ret = icn85xx_prog_i2c_txdata(cts_dev,0x4040d, &ucTemp,1);             
    if(ret){    
        cts_info("icn89xx_goto_progmode error\n");
        // retry--;
        mdelay(2); 
    }
    
    ucTemp = 0x00;               
    ret = icn85xx_prog_i2c_txdata(cts_dev,0x4040e, &ucTemp,1);             
    if(ret){    
        cts_info("icn89xx_goto_progmode error\n");
        mdelay(2); 
    }
    cts_info("icn89xx_goto_progmode over\n");

    return 0;
}
#endif

int icn89xx_read_flashstate(struct cts_device *cts_dev)
{
    int ret = -1;
    u8 ucTemp[3] = {0 , 0, 0};
    ucTemp[0] = 1;
    
    while(ucTemp[0]){
        ret = icn85xx_prog_i2c_rxdata(cts_dev,SF_BUSY_89,&ucTemp[0],1);
        if(ret){
            return ret;
        }
    }
    ucTemp[0] = FLASH_CMD_READ_STATUS;
    ret = icn85xx_prog_i2c_txdata(cts_dev,CMD_SEL_89, &ucTemp[0],1);
    if(ret){
        return ret;
    }

    ucTemp[2] = (unsigned char)(SRAM_EXCHANGE_ADDR_89>>16);
    ucTemp[1] = (unsigned char)(SRAM_EXCHANGE_ADDR_89>>8);
    ucTemp[0] = (unsigned char)(SRAM_EXCHANGE_ADDR_89);
    ret = icn85xx_prog_i2c_txdata(cts_dev,SRAM_ADDR_89, &ucTemp[0],3);
    if(ret){
        return ret;
    }
    ucTemp[0] = 1;
    ret = icn85xx_prog_i2c_txdata(cts_dev,START_DEXC_89, &ucTemp[0],1);
    if(ret){
        return ret;
    }

    while(ucTemp[0]){
        ret = icn85xx_prog_i2c_rxdata(cts_dev,SF_BUSY_89,&ucTemp[0],1);
        if(ret){
            return ret;
        }
    }
    ret = icn85xx_prog_i2c_rxdata(cts_dev,SRAM_EXCHANGE_ADDR_89,&ucTemp[0],1);
    if(ret){
        return ret;
    }
    return ucTemp[0];
}

int  icn89xx_read_flashid(struct cts_device *cts_dev )
{
    int ret = -1;
    int flash_id;
    u8 ucTemp[4];

    ucTemp[0] = FLASH_CMD_READ_IDENTIFICATION;
    ret = icn85xx_prog_i2c_txdata(cts_dev,CMD_SEL_89, ucTemp,1);
    if(ret){
        return ret;
    }
    ucTemp[2] = (unsigned char)(SRAM_EXCHANGE_ADDR1_89>>16);
    ucTemp[1] = (unsigned char)(SRAM_EXCHANGE_ADDR1_89>>8);
    ucTemp[0] = (unsigned char)(SRAM_EXCHANGE_ADDR1_89);
    ret = icn85xx_prog_i2c_txdata(cts_dev,SRAM_ADDR_89, &ucTemp[0],3);
    if(ret){
        return ret; 
    }
    ucTemp[0] = 1;
    ret = icn85xx_prog_i2c_txdata(cts_dev,START_DEXC_89, ucTemp,1);
    if(ret){
        return ret;
    }

    while(ucTemp[0]){
        ret = icn85xx_prog_i2c_rxdata(cts_dev,SF_BUSY_89,&ucTemp[0],1);
        if(ret){
            return ret;
        }
    }
    ret = icn85xx_prog_i2c_rxdata(cts_dev,SRAM_EXCHANGE_ADDR1_89, (char *)&flash_id, 3);
    if(ret){
        return ret;
    }
    /* Flash ID store in Big Endian, change to CPU endian */
    flash_id = (be32_to_cpu(flash_id)>>8) & 0xFFFFFF;
    cts_info("flashid: 0x%06x\n", flash_id);

    return flash_id;
}

unsigned int icn89xx_calculate_crc(struct cts_device *cts_dev, unsigned int len)
{
    unsigned char ucTemp[4];
    int ret = -1;
    unsigned int crc = 0;

    //2.1 set address
    ucTemp[0] = 0x00;
    ucTemp[1] = 0x00;
    ucTemp[2] = 0x00;
    ucTemp[3] = 0x00;
    ret = icn85xx_prog_i2c_txdata(cts_dev,FLASH_ADDR_89, ucTemp,4);
    if(ret){
        return ret;
    }
    //2.2 set length
    ucTemp[0] = len & 0xff;
    ucTemp[1] = (len >> 8 )& 0xff;
    ucTemp[2] = (len >> 16 )& 0xff;
    ucTemp[3] = (len >> 24 )& 0xff;
    ret = icn85xx_prog_i2c_txdata(cts_dev,DATA_LENGTH_89, ucTemp,4);
    if(ret){
        return ret;
    }
    //2.3 start calculate
    ucTemp[0] = 0x01;
    ret = icn85xx_prog_i2c_txdata(cts_dev,SW_CRC_START_89 + 2, ucTemp,1);
    if(ret){
        return ret;
    }

    //2.4 poll status
    while(ucTemp[0]){
        ret = icn85xx_prog_i2c_rxdata(cts_dev,SF_BUSY_89, ucTemp,1);
        if(ret){
            return ret;
        }
    }
    ret = icn85xx_prog_i2c_rxdata(cts_dev,CRC_RESULT_89, ucTemp,4);
    if(ret){
        return ret;
    }
    crc = ((ucTemp[3]<<24) | (ucTemp[2]<<16) | (ucTemp[1]<<8) | ucTemp[0]);
    cts_info("icn89xx_calculate_crc: 0x%x\n",crc);
    return crc;
}

static int icn89xx_erase_flash(struct cts_device *cts_dev, 
    u8 u8EraseMode, u32 u32FlashAddr)
{
    u8 ucTemp[4];
    int ret = -1;   

    //cts_info("Into icn89xx_erase_flash...");
    if(FLASH_EARSE_4K == u8EraseMode){
        ucTemp[0] = FLASH_CMD_ERASE_SECTOR;
    }else if(FLASH_EARSE_32K == u8EraseMode){
        ucTemp[0] = FLASH_CMD_ERASE_BLOCK;
    }
    ret = icn85xx_prog_i2c_txdata(cts_dev,CMD_SEL_89, ucTemp,1);
    if(ret){
        return ret;
    }

    ucTemp[3] =(u8)(u32FlashAddr >> 24);
    ucTemp[2] =(u8)(u32FlashAddr >> 16);
    ucTemp[1] =(u8)(u32FlashAddr >> 8);
    ucTemp[0] =(u8)(u32FlashAddr );
    ret = icn85xx_prog_i2c_txdata(cts_dev,FLASH_ADDR_89, ucTemp,4);
    if(ret){
        return ret;
    }
    ucTemp[0] = 1;
    ret = icn85xx_prog_i2c_txdata(cts_dev,START_DEXC_89, ucTemp,1);
    if(ret){
        return ret;
    }

    while(ucTemp[0]){
        ret = icn89xx_read_flashstate(cts_dev);
        if(ret < 0){
            return ret;
        }
        ucTemp[0] = (unsigned char)(ret&0x01);
    }

    return 1;
}

static int icn89xx_flash_write(struct cts_device *cts_dev, 
    u32 u32FlashAddr, u32 u32SramAddr, u32 u32Length)
{
    u8 ucTemp[4] = {0, 0, 0,0};
    u16 u16NotAlignLength;
    u16 i = 0;
    u32 u32FlashTempAddr = u32FlashAddr;
    u32 u32SramTempAddr = u32SramAddr;
    int ret = -1;

    //cts_info("Into icn89xx_flash_write...");
    for(i = 0; i < u32Length;){                         //should not i++
        ucTemp[0] = 1;                                   //confirm the flash whether in busy state 
        while(ucTemp[0]){
            ret = icn89xx_read_flashstate(cts_dev);
            if(ret < 0){
                return ret;
            }
            ucTemp[0] = (unsigned char)(ret&0x01);
        }
        ucTemp[3] = (u8)(u32FlashTempAddr >> 24);
        ucTemp[2] = (u8)(u32FlashTempAddr >> 16);
        ucTemp[1] = (u8)(u32FlashTempAddr >> 8);
        ucTemp[0] = (u8)(u32FlashTempAddr );   
        ret = icn85xx_prog_i2c_txdata(cts_dev,FLASH_ADDR_89, ucTemp,3);
        if(ret){
            return ret;
        }
        ucTemp[2] = (u8)(u32SramTempAddr >> 16);
        ucTemp[1] = (u8)(u32SramTempAddr >> 8);
        ucTemp[0] = (u8)(u32SramTempAddr );  
        ret = icn85xx_prog_i2c_txdata(cts_dev,SRAM_ADDR_89, ucTemp,3);
        if(ret){
            return ret;
        }
        if(u32FlashTempAddr % 0x100){  // not aglin
            u16NotAlignLength = 0x100 - (u32FlashTempAddr % 0x100);
            if(u32Length <= u16NotAlignLength){
                ucTemp[1] = (u8)(u32Length >> 8);
                ucTemp[0] = (u8)(u32Length );  
                ret = icn85xx_prog_i2c_txdata(cts_dev,DATA_LENGTH_89, ucTemp,2);
                if(ret){
                    return ret;
                }
            }else{
                ucTemp[1] = (u8)(u16NotAlignLength >> 8);
                ucTemp[0] = (u8)(u16NotAlignLength );   
                ret = icn85xx_prog_i2c_txdata(cts_dev,DATA_LENGTH_89, ucTemp,2);
                if(ret){
                    return ret;
                }
            }
            u32FlashTempAddr += u16NotAlignLength;                       //change the flash and sram address
            u32SramTempAddr += u16NotAlignLength;
            i += u16NotAlignLength;
        }else{
            if(i+256<=u32Length){
                ucTemp[1] = (u8)(0x01);
                ucTemp[0] = (u8)(0x00 ); 
                ret = icn85xx_prog_i2c_txdata(cts_dev,DATA_LENGTH_89, ucTemp,2);
                if(ret){
                    return ret;
                }
            }else{
                ucTemp[1] = (u8)((u32Length-i) >> 8);
                ucTemp[0] = (u8)(u32Length-i );   
                ret = icn85xx_prog_i2c_txdata(cts_dev,DATA_LENGTH_89, ucTemp,2);
                if(ret){
                    return ret;
                }
            }
            u32FlashTempAddr += 256;                       //change the flash and sram address
            u32SramTempAddr += 256;
            i += 256;
        }
        ucTemp[0] = FLASH_CMD_PAGE_PROGRAM;    
        ret = icn85xx_prog_i2c_txdata(cts_dev,CMD_SEL_89, ucTemp,1);
        if(ret){
            return ret;
        }
        ucTemp[0] = 1;  
        ret = icn85xx_prog_i2c_txdata(cts_dev,START_DEXC_89, ucTemp,1);
        if(ret){
            return ret;
        }
    }
     
    ucTemp[0] = 1;    
    while(ucTemp[0]){                                 //confirm whether the last pageprogram whether complete
        ret = icn89xx_read_flashstate(cts_dev);
        if(ret < 0){
            return ret;
        }
        ucTemp[0] = (unsigned char)(ret&0x01);
    }
    return ret;
}

short  icn89xx_read_fw_Ver(char *fw)
{
    short FWversion;
    char tmp[2];
    int file_size;
    file_size = icn85xx_open_fw(fw);
    if(file_size < 0){
        return -1;  
    }   
    icn85xx_read_fw(0x114, 2, &tmp[0]);
    
    FWversion = (tmp[1]<<8)|tmp[0];
    //cts_info("FWversion: 0x%x\n", FWversion);
    return FWversion; 
}

int  icn89xx_bootfrom_sram(struct cts_device *cts_dev)
{
    int ret = -1;   
    unsigned char ucTemp = 0x03;                      
    unsigned long addr = 0x40010;
    
    cts_info("icn89xx_bootfrom_sram\n");
    ret = icn85xx_prog_i2c_txdata(cts_dev,addr, &ucTemp, 1 );           //change bootmode from sram   
    return ret;
}

#if 0
static int icn89xx_bootfrom_flash(struct cts_device *cts_dev)
{
    int ret = -1; 
    unsigned char ucTemp = 0x1E;
    cts_info("icn89xx_bootfrom_flash\n");
    ret = icn85xx_prog_i2c_txdata(cts_dev,0x40008, &ucTemp, 1 );           
    return ret;
}
#endif

 unsigned int CalcCrcCheckSum(char* buf, int len)
{
    int i,pos;
    unsigned int  CRC_RESULT_F;
    char in_data_8b;
    unsigned int  crc_reg_32b;
    unsigned char xor_flag;
    
    cts_info("%s _____: 0x%x\n",__func__, len);

    CRC_RESULT_F = 0;
    for (pos = 0; pos < len; pos++){
        in_data_8b = *(buf + pos);
        crc_reg_32b = CRC_RESULT_F;
        for (i = 0; i < 32; i++){
            if ((crc_reg_32b & 0x00000001) != 0){
                crc_reg_32b ^= 0x04C11DB7;
                crc_reg_32b >>= 1;
                crc_reg_32b |= 0x80000000;
            }else{
                crc_reg_32b >>= 1;
            }
        }

        for (i = 0; i < 40; i++){
            xor_flag = (unsigned char )(crc_reg_32b >> 31);
            crc_reg_32b = (unsigned int)((crc_reg_32b << 1) + (in_data_8b >> 7));
            in_data_8b = (unsigned char)(in_data_8b << 1);
            if (xor_flag != 0){
                crc_reg_32b = crc_reg_32b ^ 0x04C11DB7;
            }
        }
        CRC_RESULT_F = crc_reg_32b;
    }
    return CRC_RESULT_F;
}

int  icnt89xx_fw_update(struct cts_device *cts_dev,const struct cts_firmware *firmware, bool to_flash)
{
    int file_size, last_length;
    int j, num;
    unsigned char  *temp_buf;
    unsigned char  temp[20];
    unsigned int crc_fw;
    //int flash_id;
    //u8 ic_type = 0;

    cts_info("Into icnt89xx_fw_update...");
    icn85xx_set_fw(firmware->data, firmware->size);	
    temp_buf = kmalloc(B_SIZE , GFP_KERNEL);
    file_size = icn85xx_open_fw(firmware->data);
    if(file_size < 0){
        icn85xx_update_status(R_FILE_ERR);
        goto updating_err;
    }
#if 0
    if(icn89xx_goto_progmode(cts_dev) != 0){
        icn85xx_update_status(R_PROGRAM_ERR);
        cts_err("icn89xx_goto_progmode() != 0 error");
        goto updating_err;
    } 
    msleep(1); 
#endif
    cts_info("icn85xx_fw_download start");
    if (file_size > B_SIZE){
        num = file_size/B_SIZE;   
    }else{
        num = 0;
    }
    crc_fw = 0;
    cts_info("icn89xx_fw_download loop:%d ",num);

    for(j=0; j < num; j++){
        icn85xx_read_fw(j*B_SIZE, B_SIZE, temp_buf);        
        crc_fw = icn85xx_crc_calc(crc_fw, temp_buf, B_SIZE);
        if(icn85xx_fw_download(cts_dev, j*B_SIZE, temp_buf, B_SIZE) != 0){
            cts_err("error j:%d",j);
            icn85xx_update_status(R_PROGRAM_ERR);
            goto updating_err;
        }    
        icn85xx_update_status(5+(int)(60*j/num));
    }

    last_length = file_size - B_SIZE*j;
    cts_info("icn89xx_fw_download last_length:%d ",last_length);

    if(last_length > 0){
        icn85xx_read_fw(j*B_SIZE, last_length, temp_buf);
        crc_fw = icn85xx_crc_calc(crc_fw, temp_buf, last_length);
        if(icn85xx_fw_download(cts_dev, j*B_SIZE, temp_buf, last_length) != 0){
            cts_err("error last length");
            icn85xx_update_status(R_PROGRAM_ERR);
            goto updating_err;
        }
    } 
    
    if(!to_flash){
        #if 0
        if(icn89xx_bootfrom_sram(cts_dev) != 0){
            cts_err("firmware download to SRAM, icn89xx_bootfrom_sram error");
            icn85xx_update_status(R_STATE_ERR);
            goto updating_err;
        } 
        #endif
        kfree(temp_buf);
        cts_info("firmware download to SRAM OK!");
        return R_OK;
    }
    if(!cts_dev->hwdata->is_with_flash){
        #if 0
        if(icn89xx_bootfrom_sram(cts_dev) != 0){
            cts_err("chip without flash, icn89xx_bootfrom_sram error");
            icn85xx_update_status(R_STATE_ERR);
            goto updating_err;
        } 
        #endif
        kfree(temp_buf);
        cts_info("chip without flash, firmware update OK!");
        return R_OK;
    }
    // crc_fw = CalcCrcCheckSum(temp_buf,file_size);

    icn85xx_update_status(65);
    cts_info("crc_fw: 0x%x", crc_fw);
    msleep(1);
    //earase 0xe000
    icn89xx_erase_flash(cts_dev, 0, 0xe000);
    // earase flash 
    for ( j = 0; j <( 0xA000/(4*1024)); ++j){
        icn89xx_erase_flash(cts_dev, 0,j*(4*1024));
    }
    
    // write  code sram to flash 
    icn89xx_flash_write(cts_dev, 0, 0, file_size);
    // write len to sram
    temp[0] = (unsigned char) file_size;
    temp[1] = (unsigned char) (file_size >> 8);
    temp[2] = (unsigned char) (file_size >> 16);
    temp[3] = (unsigned char) (file_size >> 24);
    icn85xx_prog_i2c_txdata(cts_dev,0 , temp ,4);
    if( icn89xx_calculate_crc(cts_dev, file_size) != crc_fw){
        cts_err("crc error");
        goto updating_err;
    }
    // write crc to sram 
    temp[0] = (unsigned char)crc_fw;
    temp[1] = (unsigned char)(crc_fw >> 8);
    temp[2] = (unsigned char)(crc_fw >> 16);
    temp[3] = (unsigned char)(crc_fw >> 24);
    icn85xx_prog_i2c_txdata(cts_dev,0x100 , temp ,4);

    // write crc to flash 
    icn89xx_flash_write(cts_dev,0xe000,0x100,4);
    icn89xx_flash_write(cts_dev,0xe00c,0x100,4);
    icn89xx_flash_write(cts_dev,0xe004,0x0,4);
    icn89xx_flash_write(cts_dev,0xe010,0x0,4);

    // write crc to sram 
    crc_fw = 0x0000C35A;
    temp[0] = (unsigned char)crc_fw;
    temp[1] = (unsigned char)(crc_fw >> 8);
    temp[2] = (unsigned char)(crc_fw >> 16);
    temp[3] = (unsigned char)(crc_fw >> 24);
    icn85xx_prog_i2c_txdata(cts_dev,0x00 , temp ,4);
    icn89xx_flash_write(cts_dev,0xe008,0x00,4);
    #if 0
    if(icn89xx_bootfrom_flash(cts_dev) != 0){
        cts_err("firmware download to flash, icn89xx_bootfrom_flash error");
        icn85xx_update_status(R_STATE_ERR);
        goto updating_err;
    } 
    #endif
    cts_info("icn89xx upgrade ok size : %d",file_size);
    kfree(temp_buf);
    return R_OK;

updating_err:
    kfree(temp_buf);
    return R_STATE_ERR;
}
