"""Compile the actual application's RAM-command helpers against the HAL stub."""
from pathlib import Path
import subprocess
import tempfile

main = Path('Core/Src/main.c').read_text()
variables = main[main.index('volatile struct {'):main.index('static uint32_t mq6_count')]
helpers = main[main.index('static uint8_t RTC_LoadTarget'):main.index('static void OLED_ShowTemperature')]
stub = Path('tests/ds3231_test.c').read_text().split('int main(void)')[0]
checks = r'''
I2C_HandleTypeDef hi2c1;
'''+variables+helpers+r'''
int main(void) {
    RTC_SetService(); assert(writes==0 && rtc_set_request.state==0);
    rtc_set_request.command=0x53455431;
    RTC_SetService(); assert(rtc_set_request.error==6 && writes==0);
    rtc_set_request.year=2024; rtc_set_request.month=2; rtc_set_request.date=30;
    rtc_set_request.command=0x41524D31;
    RTC_SetService(); assert(rtc_set_request.error==5 && writes==0);
    rtc_set_request.date=29;
    rtc_set_request.hour=12; rtc_set_request.minute=10;
    rtc_set_request.command=0x41524D31;
    RTC_SetService(); assert(rtc_set_request.state==1 && rtc_target.day==4);
    tick+=60000;
    RTC_SetService(); assert(rtc_set_request.error==6 && writes==0);
    rtc_set_request.command=0x41524D31;
    RTC_SetService(); assert(rtc_set_request.state==1);
    rtc_set_request.date=31; /* ARM snapshot cannot be changed by target edits. */
    regs[15]=0x88;
    rtc_set_request.command=0x53455431;
    RTC_SetService(); assert(rtc_set_request.state==2 && writes==1);
    tick+=1100; regs[0]=1;
    RTC_SetService(); assert(rtc_set_request.state==3 && writes==2);
    rtc_set_request.command=0x41524D31;
    RTC_SetService(); assert(writes==2 && rtc_set_request.state==3);
    rtc_set_request.command=0x53455431;
    RTC_SetService(); assert(writes==2);
    puts("RTC ARM/TRIGGER tests passed");
}
'''
with tempfile.TemporaryDirectory() as d:
    source=Path(d)/'commands.c'; source.write_text(stub+checks)
    exe=Path(d)/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
                    '-Itests/rtc_stub','-ICore/Inc',str(source),
                    'Core/Src/ds3231.c','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
