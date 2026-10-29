"""Compile the actual main.c DS18B20 decode/acquisition functions against a bus stub.
No production extraction/rewrite: tests stay in /tmp and do not touch hardware.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
s = (root / 'Core/Src/main.c').read_text()
enum = s[s.index('typedef enum'):s.index('} DS18B20_Status;') + len('} DS18B20_Status;')]
decode = s[s.index('static uint8_t DS18B20_CRC8'):s.index('/* GPIO adapter')]
read = s[s.index('static DS18B20_Status DS18B20_ReadTemperature'):s.index('/* ADC1 is dedicated')]
stub = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
static uint8_t conversion_busy_seen,conversion_done,scratchpad_crc_calculated,scratchpad_crc_received;
static uint8_t onewire_timing_fault,onewire_ready;
static uint32_t tick,services;
static unsigned presence_calls,read_bits,index_byte,fail_presence,fail_write,fail_read,done=1,busy=1;
static uint8_t frame[9];
static uint32_t HAL_GetTick(void) {return tick;}
static void HAL_Delay(uint32_t ms) {tick+=ms;}
static void Heartbeat_Service(void) {++services;}
static uint8_t OneWire_Presence(void) {return ++presence_calls!=fail_presence;}
static uint8_t OneWire_WriteByte(uint8_t v) {(void)v; return !fail_write;}
static uint8_t OneWire_ReadBit(uint8_t *v) {*v=read_bits++ ? done:!busy; return !fail_read;}
static uint8_t OneWire_ReadByte(uint8_t *v) {*v=frame[index_byte++]; return !fail_read;}
'''
tests = r'''
static void Setup(int16_t raw) {
    tick=services=presence_calls=read_bits=index_byte=fail_presence=fail_write=fail_read=0;
    busy=done=onewire_ready=1;
    frame[0]=(uint16_t)raw&255;frame[1]=(uint16_t)raw>>8;
    frame[2]=75;frame[3]=70;frame[4]=0x7F;frame[5]=0xFF;frame[6]=12;frame[7]=16;
    frame[8]=DS18B20_CRC8(frame,8);
}
static void Reject(DS18B20_Status expected) {
    int16_t out=12345;assert(DS18B20_ReadTemperature(&out)==expected);assert(out==12345);
}
int main(void) {
    int16_t out;
    Setup(-120);assert(DS18B20_ReadTemperature(&out)==DS_OK && out==-75);
    assert(tick==800 && services==800 && index_byte==9 && conversion_done);
    Setup(400);fail_presence=1;Reject(DS_E1_PRESENCE);
    Setup(400);done=0;Reject(DS_E2_CONVERSION);
    Setup(400);fail_presence=2;Reject(DS_E3_PRESENCE);
    Setup(400);frame[8]^=1;Reject(DS_E4_CRC);
    Setup(126*16);Reject(DS_E5_READING);
    Setup(-56*16);Reject(DS_E5_READING);
    Setup(85*16);busy=0;Reject(DS_E5_READING);
    Setup(85*16);assert(DS18B20_ReadTemperature(&out)==DS_OK && out==850);
    Setup(0);for(unsigned i=0;i<9;i++)frame[i]=0;Reject(DS_E5_READING);
    Setup(0);for(unsigned i=0;i<9;i++)frame[i]=255;Reject(DS_E4_CRC);
    Setup(400);onewire_ready=0;Reject(DS_E6_INTERNAL);
    Setup(400);fail_write=1;Reject(DS_E6_INTERNAL);
    Setup(400);fail_read=1;Reject(DS_E6_INTERNAL);
    puts("Actual DS18B20 CRC/decode/error propagation and serviced conversion wait passed");
}
'''
with tempfile.TemporaryDirectory(prefix='lpg-ds-test-') as tmp:
    c = Path(tmp) / 'test.c'; binary = Path(tmp) / 'test'
    c.write_text(stub + enum + decode + read + tests)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
