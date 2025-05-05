#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else
#include "USB.h"
#include "USBMSC.h"
#include "SD.h"
#include "SPI.h"
#include "SdFat.h"
#include "Timer.h"

#define SPI_CS                  10
#define SPI_MOSI                11
#define SPI_MISO                13
#define SPI_SCK                 12

SdFat sd;
Timer timer;

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
USBCDC USBSerial;
#endif

USBMSC MSC;

static const uint16_t DISK_SECTOR_SIZE = 512;    // Should be 512
static uint32_t sectors = 0;
static uint32_t readCounter, writeCounter, busyCounter = 0;

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize){
  //HWSerial.printf("MSC WRITE: lba: %u, offset: %u, bufsize: %u\n", lba, offset, bufsize);
  if(sd.card()->isBusy())
    busyCounter++;
  //wait Method
  while(sd.card()->isBusy());
  if(bufsize > DISK_SECTOR_SIZE)
  { 
    writeCounter += bufsize/DISK_SECTOR_SIZE;
    return sd.card()->writeSectors( lba, (uint8_t*)buffer, bufsize/DISK_SECTOR_SIZE)? bufsize: -1;  
  }
  else
  {
    writeCounter++;    
    return sd.card()->writeSector( lba, (uint8_t*)buffer) ? bufsize : -1;
  }
  //return bufsize;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize){
  //HWSerial.printf("MSC READ: lba: %u, offset: %u, bufsize: %u\n", lba, offset, bufsize);
  if(sd.card()->isBusy())
    busyCounter++;
  while(sd.card()->isBusy());
  if(bufsize > DISK_SECTOR_SIZE)
  {
    readCounter += bufsize/DISK_SECTOR_SIZE;
    return sd.card()->readSectors( lba, (uint8_t*)buffer, bufsize/DISK_SECTOR_SIZE)? bufsize: -1; 
  }
  else
  {
    readCounter++;
    return sd.card()->readSector( lba, (uint8_t*)buffer)? bufsize : -1;
  }
  //return bufsize;
  
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void sdcard_flush_cb (void)
{
  if(sd.card()->isBusy())
    busyCounter++;
  while(sd.card()->isBusy());
  sd.card()->syncDevice();

  HWSerial.printf("sdcard_flush_cb");

}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject){
  HWSerial.printf("MSC START/STOP: power: %u, start: %u, eject: %u\n", power_condition, start, load_eject);
  return true;
}

static void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
  if(event_base == ARDUINO_USB_EVENTS){
    arduino_usb_event_data_t * data = (arduino_usb_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_STARTED_EVENT:
        HWSerial.println("USB PLUGGED");
        break;
      case ARDUINO_USB_STOPPED_EVENT:
        HWSerial.println("USB UNPLUGGED");
        break;
      case ARDUINO_USB_SUSPEND_EVENT:
        HWSerial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
        break;
      case ARDUINO_USB_RESUME_EVENT:
        HWSerial.println("USB RESUMED");
        break;
      
      default:
        break;
    }
  }
}

void setup() {
  HWSerial.begin(115200);
  HWSerial.setDebugOutput(true);

//sd
  HWSerial.print("SD init ... ");
  pinMode(SPI_CS, OUTPUT);      digitalWrite(SPI_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SdSpiConfig spiConfig(SPI_CS, SHARED_SPI, SD_SCK_MHZ(10)); // 10 MHz
  if (!sd.begin(spiConfig)) {
      HWSerial.println("sd.begin() failed ...");
  }
  HWSerial.println("done");

  sectors = sd.card()->sectorCount();
  HWSerial.print("Sectors: ");
  HWSerial.println(sectors);

  USB.onEvent(usbEventCallback);
  MSC.vendorID("USBMSC");//max 8 chars
  MSC.productID("USBMSC");//max 16 chars
  MSC.productRevision("1.0");//max 4 chars
  MSC.onStartStop(onStartStop);
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);
  MSC.mediaPresent(true);
  MSC.begin(sectors, DISK_SECTOR_SIZE);
  USBSerial.begin();
  USB.begin();
  timer.setInterval(1000); // Đặt thời gian là 1 giây (1000 ms)
  timer.setCallback(printReadWriteCounter); // Đặt callback function
  timer.start();
}

void printReadWriteCounter( void )
{
  HWSerial.printf("ReadCounter: %d WriteCounter: %d, BusyCounter: %d\r\n", readCounter, writeCounter, busyCounter);
}
void loop() {
  // put your main code here, to run repeatedly:
  timer.update();
}
#endif /* ARDUINO_USB_MODE */
