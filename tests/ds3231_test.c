#include "ds3231.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t regs[16] = {0x59,0x58,0x23,7,0x29,0x02,0x24};
static HAL_StatusTypeDef probe = HAL_OK, io = HAL_OK;
static unsigned writes;
static uint8_t fail_write, fail_osf, corrupt_read;
static uint32_t tick;
uint32_t HAL_GetTick(void) { return tick; }
static uint32_t probe_error = HAL_I2C_ERROR_AF;
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *b, uint16_t a, uint32_t n, uint32_t t)
{ (void)b; assert(a == 0xD0 && n == 1 && t == 10); return probe; }
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *b) { (void)b; return probe_error; }
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *b, uint16_t a, uint16_t r, uint16_t z, uint8_t *p, uint16_t n, uint32_t t)
{ (void)b; assert(a == 0xD0 && z == 1 && t == 10 && r+n <= 16); if(io == HAL_OK) { memcpy(p,regs+r,n); if (corrupt_read && r == 0) p[0] ^= 1U; } return io; }
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *b, uint16_t a, uint16_t r, uint16_t z, uint8_t *p, uint16_t n, uint32_t t)
{ (void)b; assert(a == 0xD0 && z == 1 && t == 10 && r+n <= 16); ++writes; if (fail_write || (fail_osf && r == 15)) return HAL_ERROR; memcpy(regs+r,p,n); return io; }
int main(void)
{
    I2C_HandleTypeDef bus = {0}; DS3231_Reading rd; DS3231_Time time;
    assert(DS3231_Read(&bus,&rd) == DS3231_OK && rd.time.year == 2024);
    regs[2]=0x52; assert(DS3231_Decode(regs,&time) && time.hour==0);
    regs[2]=0x72; assert(DS3231_Decode(regs,&time) && time.hour==12);
    regs[2]=0x61; assert(DS3231_Decode(regs,&time) && time.hour==13);
    regs[2]=0x40; assert(!DS3231_Decode(regs,&time));
    regs[2]=0x24; assert(!DS3231_Decode(regs,&time)); regs[2]=0x23;
    regs[6]=0x23; assert(!DS3231_Decode(regs,&time));
    regs[6]=0; regs[5]=0x82; assert(!DS3231_Decode(regs,&time)); /* 2100 not leap */
    regs[4]=0x28; assert(DS3231_Decode(regs,&time) && time.year==2100);
    regs[0]=0x5A; assert(!DS3231_Decode(regs,&time)); regs[0]=0x59;
    regs[3]=0; assert(!DS3231_Decode(regs,&time)); regs[3]=1;
    regs[15]=0x80; assert(DS3231_Read(&bus,&rd)==DS3231_TIME_NOT_SET && rd.osf);
    assert(rd.time.year==0);
    regs[15]=0; regs[4]=0x31; assert(DS3231_Read(&bus,&rd)==DS3231_INVALID_DATA);
    io=HAL_TIMEOUT; assert(DS3231_Read(&bus,&rd)==DS3231_IO_ERROR && !rd.status_valid); io=HAL_OK;
    probe=HAL_ERROR; assert(DS3231_Read(&bus,&rd)==DS3231_NO_ACK); probe_error=0; assert(DS3231_Read(&bus,&rd)==DS3231_NO_ACK);
    probe=HAL_BUSY; assert(DS3231_Read(&bus,&rd)==DS3231_IO_ERROR); probe=HAL_OK;
    assert(writes==0); /* Normal read path never writes even if OSF or fields invalid. */
#if DS3231_ENABLE_SET
    DS3231_Time requested={2026,10,18,1,10,30,0};
    regs[15]=0x88;
    assert(DS3231_SetUnsetClock(&bus,&requested)==DS3231_VERIFYING && writes==1);
    assert(regs[15]==0x88);
    assert(DS3231_ServiceSet(&bus)==DS3231_VERIFYING);
    tick=1100; regs[0]=1;
    assert(DS3231_ServiceSet(&bus)==DS3231_OK && writes==2);
    assert(regs[15]==0x0B && regs[6]==0x26 && regs[2]==0x10);
    assert(DS3231_SetUnsetClock(&bus,&requested)==DS3231_ALREADY_SET && writes==2);
    regs[15]=0x88;
    assert(DS3231_SetUnsetClock(&bus,&requested)==DS3231_VERIFYING);
    tick+=1100;
    assert(DS3231_ServiceSet(&bus)==DS3231_NOT_RUNNING && regs[15]==0x88);
    fail_write=1;
    assert(DS3231_SetUnsetClock(&bus,&requested)==DS3231_WRITE_ERROR);
    fail_write=0; corrupt_read=1;
    assert(DS3231_SetUnsetClock(&bus,&requested)==DS3231_VERIFY_ERROR);
    corrupt_read=0;
    assert(DS3231_SetUnsetClock(&bus,&requested)==DS3231_VERIFYING);
    tick+=1100; regs[0]=1; fail_osf=1;
    assert(DS3231_ServiceSet(&bus)==DS3231_OSF_ERROR && regs[15]==0x88);
    fail_osf=0;
    unsigned prior_writes=writes;
    requested.month=13;
    assert(DS3231_SetUnsetClock(&bus,&requested)==DS3231_INVALID_DATA && writes==prior_writes);
#endif
    puts("DS3231 tests passed");
}
