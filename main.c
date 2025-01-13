#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"
#include "supporting_functions.h"
#include "message_buffer.h"
#include <stdio.h>
#include <string.h>


//typedef void* MessageBufferHandle_t;
#define mainAUTO_RELOAD_TIMER_PERIOD pdMS_TO_TICKS( 5000UL )
#define mainINTERRUPT_NUMBER	3

static void prvTimerCallback(TimerHandle_t xTimer);

static void vPeriodicTaskEntry(void* pvParameters);
static void vServerTask(void* pvParameters);

static void vPeriodicTaskTolling(void* pvParameters);
static void vEntry_Handler(void* pvParameters);
static void ulEntryInterruptHandler(void* pvParameters);

static void  vTollingHandler(void* pvParameters);
static void vExitHandler(void* pvParameters);

static void prvSRand(uint32_t ulSeed);


static TimerHandle_t xProxyTimer;


static uint32_t prvRand(void);
static uint32_t ulBitsToDecimal(EventBits_t Bits);
static uint32_t ulNextRand;//下一個隨機數

QueueHandle_t xParkingQueue;
QueueHandle_t xCarIDandTime_Queue;//for queue handle
const TickType_t xDelay300ms = pdMS_TO_TICKS(300UL);
/*-----------------------------------------------------------*/

TaskHandle_t xEntry_HandlerTask;
TaskHandle_t xTollingHandlerTask;


enum State { eAvailable, eOccupied };
typedef struct Parking_Lot_t { //每格停車位紀錄了 現在狀態、車牌、進場時間
    enum State eState;
    uint32_t ulCarID;
    TickType_t xEntryTime;
}xPark_Lot;

typedef struct Parking_Area {//一個停車場
    xPark_Lot xLots[24]; //有24個停車位
    SemaphoreHandle_t xParkingMutex;//用來控管xLots的互斥存取，確保不會有人車還沒離開，就被停進去
    EventBits_t xParkingEventGroup;
}xArea;

typedef struct IDandTime {
    int32_t rdmCarID;
    TickType_t xTimeNow;
}IDandTime;
typedef struct IDandEntryExitTime {
    int32_t lCarID;
    TickType_t xEntryTime;
    TickType_t xExitTime;
}IDandEntryExitTime;
static int32_t lrdm_xLots_Avail_index(xPark_Lot xLots[]);
static int32_t lrdm_xLots_Occup_index(xPark_Lot xLots[]);
MessageBufferHandle_t xParkingMessageBuffer;

EventGroupHandle_t xEventGroup;
xArea ParkingArea_1;
int animationmode = 0;
const TickType_t xTicksToWait = pdMS_TO_TICKS(100UL);
int main(void)
{
    BaseType_t xTimer1Started, xTimer2Started;//看看有沒有創建成功
    vPrintString("學號:0112209178 作者:賴育晟\n");
    vPrintString("歡迎來到停車場管理系統，請選擇操作模式\n");
    vPrintString("輸入0，為普通顯示模式\n");
    vPrintString("輸入1，為高級顯示模式\n");
    scanf_s("%d",&animationmode);


    xProxyTimer = xTimerCreate("AutoReload Timer",
        mainAUTO_RELOAD_TIMER_PERIOD,
        pdTRUE,
        /* 計時器的 ID 初始值為 0。 */
        0,
        /* 使用 prvTimerCallback() 作為計時器的回呼函式 */
        prvTimerCallback);
    ParkingArea_1.xParkingMutex = xSemaphoreCreateMutex();

    // xParkingMessageBuffer = xQueueCreate(24, sizeof(char[100]));h
    xParkingMessageBuffer = xMessageBufferCreate(sizeof(char)*100*5);
    if (xParkingMessageBuffer) {
        //vPrintString("MessaeBuffer創建成功\n");
    }
    else {
        vPrintString("MessaeBuffer創建失敗\n");
    }

    xCarIDandTime_Queue = xQueueCreate(5, sizeof(IDandTime));
    xParkingQueue = xQueueCreate(5, sizeof(IDandEntryExitTime));
    xEventGroup = xEventGroupCreate();
   // xTaskCreate(vTollMachineWarmupTask, "TollMachineWarmup", 1000, NULL, 3, NULL);
    xTaskCreate(vPeriodicTaskEntry, "Periodic Interrupt Generator", 1000, NULL, 1, NULL);//定期丟出中斷 (透過block自己)
    xTaskCreate(vPeriodicTaskTolling, "Periodic Tolling ", 1000, NULL, 1, NULL); //定期通知TollingHandler(透過block自己)


    xTaskCreate(vEntry_Handler, "Handle Notification Receiver", 1000, NULL, 2, &xEntry_HandlerTask);

    xTaskCreate(vTollingHandler, "Tolling Receiver", 1000, NULL, 2, &xTollingHandlerTask);//TollingHandler收到通知才做事
    //這個任務沒收到通知會block自己到maxtime

    xTaskCreate(vExitHandler, "Exit handle", 1000, NULL, 2, NULL);
    xTaskCreate(vServerTask, " ServerTask", 1000, NULL, 2, NULL);


    vPortSetInterruptHandler(mainINTERRUPT_NUMBER, ulEntryInterruptHandler);
    xTimerStart(xProxyTimer, 0);
    vTaskStartScheduler();
    while (1) {

    }
    return 0;
}
static void vPeriodicTaskEntry(void* pvParameters) {
    int isFirstTime = 1;
    for (;; )
    {
        vTaskDelay(pdMS_TO_TICKS(1000UL));
        TickType_t xInterruptFrequency = pdMS_TO_TICKS(prvRand() % 2000 + 1000);
        if (!isFirstTime) {
            printf("下次新車開進來的間隔毫秒: %d\n", xInterruptFrequency*10);
            vTaskDelay(xInterruptFrequency);//延遲一下
        }
        isFirstTime = 0;
        // vPrintString("Periodic task - About to generate an interrupt.\r\n");
        vPortGenerateSimulatedInterrupt(mainINTERRUPT_NUMBER);//放出中斷! (會直接跳轉到ulExampleInterruptHandler中斷處理程式)
        //vPrintString("Periodic task - Interrupt generated.\r\n\r\n\r\n");
    }

};
static void vPeriodicTaskTolling(void* pvParameters) {
    int isFirstTime = 1;
    if(!animationmode)
    vPrintString("繳費機熱機中，等待10秒\n");
    else {
        vPrintString("=====================\n");
        vPrintString("|  繳費機熱機啟動中 |\n");
        vPrintString("=====================\n");
        vPrintString("|     _________     |\n");
        vPrintString("|    |         |    |\n");
        vPrintString("|    |_________|    |\n");

        vPrintString("|   [ 熱機中... ]   |\n");

        vPrintString("|   [ 請等待10秒]   |\n");
        vPrintString("=====================\n");
    }
    vTaskDelay(pdMS_TO_TICKS(10000UL));//延遲一下
    int lOccup_index;
    for (;; )
    {
        if (!animationmode) {
            if (isFirstTime) vPrintString("10秒到了，繳費機熱機完成，開始啟動\n");
        }
        else {
            if (isFirstTime) {
                vPrintString("=====================\n");
                vPrintString("|  繳費機熱機完成    |\n");
                vPrintString("=====================\n");
                vPrintString("|     _________     |\n");
                vPrintString("|    |         |    |\n");
                vPrintString("|    |_________|    |\n");

                vPrintString("|   [ 啟動成功 ]   |\n");

                vPrintString("|   [ 10秒已完成]   |\n");
                vPrintString("=====================\n");
            }
        }


        TickType_t xInterruptFrequency = pdMS_TO_TICKS(prvRand() % 2000 + 1000);
        if (!isFirstTime)
            vTaskDelay(xInterruptFrequency);//延遲一下
        isFirstTime = 0;

        //if (xSemaphoreGive(ParkingArea_1.xParkingMutex))//塞得進進信號量的話(表示資源沒滿)
        xSemaphoreTake(ParkingArea_1.xParkingMutex, portMAX_DELAY);
        //挑一個空位把車停進去 
        lOccup_index = lrdm_xLots_Occup_index(ParkingArea_1.xLots);
        if (!animationmode) {
            vPrintString("\n繳費機做事(有人繳費)\n");
           // vPrintString("\n  Tolling notify and send Lots Number to handler\r\n");
        }
        else {
            vPrintString("      ________          \n");
            vPrintString("     |        |    ●\n");
            vPrintString("     | 繳費中 |   /│\\\n");
            vPrintString("     |________|   / \\\n");
           
          
        }

        if(lOccup_index!=-1)
        xTaskNotify(xTollingHandlerTask, lOccup_index, eSetValueWithOverwrite);
        xSemaphoreGive(ParkingArea_1.xParkingMutex);


    }

};
static void  vTollingHandler(void* pvParameters) {

    int lreceive_index;
    const TickType_t xMaxExpectedBlockTime = pdMS_TO_TICKS(500UL);

    while (1) {

        if (xTaskNotifyWait(0, 0, &lreceive_index, xMaxExpectedBlockTime) == pdPASS) {
            //不清除收到的通知值的任何位
            if (!animationmode) {
               // vPrintStringAndNumber("  Receive Lots Number: ", lreceive_index);
                //vPrintStringAndNumber("  收到 Ocuppied: ", lreceive_index);
               // vPrintString("\n");
            }
            xEventGroupClearBits(xEventGroup, 0xFFFFFF);//清掉24位
            xEventGroupSetBits(xEventGroup, (1UL << lreceive_index));
           // vPrintString("  Updating global ParkingEventGroup\n");
            ParkingArea_1.xParkingEventGroup = xEventGroupGetBits(xEventGroup);
            //vPrintStringAndNumber("現在Event Group的值: ", ParkingArea_1.xParkingEventGroup);
        }
    }

};
static void ulEntryInterruptHandler(void* pvParameters)
{
    BaseType_t xStatus;
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;

    IDandTime IDandTimeToSend = { prvRand() % 9000 + 1000,xTaskGetTickCount() };
    xStatus = xQueueSendToBack(xCarIDandTime_Queue, &IDandTimeToSend, 0);
    //要插入的queue handle. 從哪裡來的data的址, 插不進去願意等多久
    if (xStatus != pdPASS)
    {
        vPrintString("Could not send to the queue.\r\n");
    }
    else {
    }
    vTaskNotifyGiveFromISR(xEntry_HandlerTask, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void vEntry_Handler(void* pvParameters)
{
    int32_t llots_index;
    const TickType_t xMaxExpectedBlockTime = pdMS_TO_TICKS(500UL);
    uint32_t ulEventsToProcess;
    int32_t lReceivedCarID;
    TickType_t xReceivedTime;
    BaseType_t xStatus;
    IDandTime IDandTimeReceive;
    for (;; )
    {
        ulEventsToProcess = ulTaskNotifyTake(pdTRUE, xMaxExpectedBlockTime);//可以知道這個任務收到通知了沒
        //如果收到通知ulEventsToProcessc會++(可以知道收到幾次通知)
        if (ulEventsToProcess != 0)
        {
            if (uxQueueMessagesWaiting(xCarIDandTime_Queue) != 0)//查有幾個item在queue裡面
            {
                //  vPrintString("Queue should have been empty!\r\n");
            }
            while (ulEventsToProcess > 0)
            {
                xStatus = xQueueReceive(xCarIDandTime_Queue, &IDandTimeReceive, xTicksToWait);//回傳是否收到 收不到最多等100ms

                if (xStatus == pdPASS)//有收到!
                {
                    lReceivedCarID = IDandTimeReceive.rdmCarID;
                    xReceivedTime = IDandTimeReceive.xTimeNow;
                    xSemaphoreTake(ParkingArea_1.xParkingMutex, portMAX_DELAY); //挑一個空位把車停進去
                    llots_index = lrdm_xLots_Avail_index(ParkingArea_1.xLots);
                    ParkingArea_1.xLots[llots_index].ulCarID = lReceivedCarID;
                    ParkingArea_1.xLots[llots_index].xEntryTime = xReceivedTime;
                    if (!animationmode) {
                        vPrintString("\n進場閘門做事(每1~3秒有車進來)\n");
                        vPrintString("Car is coming! \n");
                        vPrintStringAndNumber("  Car ID:  ", ParkingArea_1.xLots[llots_index].ulCarID);
                        //vPrintString("  ");
                        vPrintStringAndNumber("  Entry Time:  ", ParkingArea_1.xLots[llots_index].xEntryTime);
                        //vPrintString("  ");
                        vPrintStringAndNumber("  Lots Number:  ", llots_index);
                    }
                    else {
                        vPrintString("\n              ||||||||||||                              __________________    \n");
                        vPrintString("              ||      ||                               |                 |    \n");
                        printf("            車||   閘 ||          ____                 |Entry Time: %d      \n", ParkingArea_1.xLots[llots_index].xEntryTime);
                        
                        vPrintString("_______     來||======||     .__.'/||_|\\               |_________________|    \n");
                        vPrintString("|停車格|    了||   門 ||    /_");
                        printf("%d_     )                    ||||           \n", ParkingArea_1.xLots[llots_index].ulCarID);
                        printf("|  %d  |", llots_index);
                        vPrintString("      ||      ||   /_ _    _    )  ======            ||||           \n");
                        vPrintString("              ||      ||    '-(_)--(_)='                    _||||_          \n");
                        vPrintString("              ||||||||||||\n");
                       
                                  
                           
                            
                    }

                    //vPrintString("  \n");
                    xSemaphoreGive(ParkingArea_1.xParkingMutex);

                }
                else
                {
                    //   vPrintString("Could not receive from the queue.\r\n");
                }

                //vPrintString("Handler task - Processing event.\r\n");
                ulEventsToProcess--;//(直到把收到的通知數量消耗完)
            }
        }
        else
        {

        }
    }
}
static uint32_t prvRand(void)
{
    const uint32_t ulMultiplier = 0x015a4e35UL, ulIncrement = 1UL;
    uint32_t ulReturn;

    /*用於生成偽隨機數的實用函數，因為 MSVC rand() 函數具有意想不到的後果。*/
    taskENTER_CRITICAL();
    ulNextRand = (ulMultiplier * ulNextRand) + ulIncrement;
    ulReturn = (ulNextRand >> 16UL) & 0x7fffUL;
    taskEXIT_CRITICAL();
    return ulReturn;
}

static void prvSRand(uint32_t ulSeed)
{
    /*為偽隨機數生成器提供種子的實用函數。*/
    ulNextRand = ulSeed;
}
static int32_t lrdm_xLots_Avail_index(xPark_Lot xLots[]) {
    int i = 0;
    int Avail_index = 0;
    int Available[24] = { 0 };
    int Lot_index = 0;
    //int Occupied[24];
    for (i; i < 24; i++) {
        if (xLots[i].eState == eAvailable) {
            Available[Avail_index] = i;
            Avail_index++;
        }

    }
    Lot_index = Available[prvRand() % Avail_index];
    xLots[Lot_index].eState = eOccupied;
    return Lot_index;


};
static int32_t lrdm_xLots_Occup_index(xPark_Lot xLots[]) {
    int i = 0;
    int Occup_index = 0;


    int Occupied[24] = { 0 };
    for (i; i < 24; i++) {
        if (xLots[i].eState == eOccupied) {
            Occupied[Occup_index] = i;
            Occup_index++;
        }

    }
    if (Occup_index == 0) {
        return -1;
    }
    Occup_index = Occupied[prvRand() % Occup_index];
    xLots[Occup_index].eState = eAvailable;
    return Occup_index;


};
static void vExitHandler(void* pvParameters) {
    const EventBits_t xBitsToWaitFor = 0xffffff;//24位元皆為1
    // EventBits_t xEventGroupValue;//這個值會每一輪更新，紀錄了現在的bit狀態
    int32_t lExit_index;

    IDandEntryExitTime IDandEntryExitTime1;
    BaseType_t xStatus;
    for (;; )
    {
       
        ParkingArea_1.xParkingEventGroup = xEventGroupWaitBits(
            xEventGroup,//讀的事件組
            xBitsToWaitFor,//希望有人符合這個
            pdFALSE,//pdTRUE：滿足條件後，將 uxBitsToWaitFor 中的位清零。
            pdFALSE,//true :uxBitsToWaitFor中是1的位在xEventGroup都要1  false : uxBitsToWaitFor中是1的位在xEventGroup有一項1就可以
            portMAX_DELAY);
        //超直白解釋，除非xEventGroupn所屬的bit符合xBitsToWaitFor，不然會這個task會一直卡在這裡!block

        //如果卡住 函數不會立即返回，所以沒有回傳值
        //若是通過，可以進一步判斷是因為哪一個位元通過 值是xEventGroup所屬的值
        //xEventGroupWaitBits這個api會一直在背景偷偷運作
        lExit_index = ParkingArea_1.xParkingEventGroup;
        xEventGroupClearBits(xEventGroup, 0xFFFFFF);//清掉24位
        ParkingArea_1.xParkingEventGroup = xEventGroupGetBits(xEventGroup);
        lExit_index = ulBitsToDecimal(lExit_index);
       
        xSemaphoreTake(ParkingArea_1.xParkingMutex, portMAX_DELAY); 
        IDandEntryExitTime1.lCarID = ParkingArea_1.xLots[lExit_index].ulCarID;
        IDandEntryExitTime1.xEntryTime = ParkingArea_1.xLots[lExit_index].xEntryTime;
        IDandEntryExitTime1.xExitTime = xTaskGetTickCount();
        if (!animationmode) {
            vPrintString("\n離場閘門做事(因為有人繳費)\n");
           // vPrintString("Car is exiting!\n");
           // vPrintStringAndNumber("Lots_number: ", lExit_index);
        }
        else {
            vPrintString("                              |||||||||||||             __________________     \n");
            vPrintString("                ____             ||      ||            |                 |\n");
            vPrintString("_______       /|_ || \\`.__       ||      ||車          ");
            printf("|Exit Time: %d      \n", IDandEntryExitTime1.xExitTime);

            vPrintString("|停車格|      (_    _ _    \\     ||======||走          |_________________| \n");
                  printf("|  %d  |     (_ _  _  %d \\     ||      ||了                  ||||  \n", lExit_index, ParkingArea_1.xLots[lExit_index].ulCarID);
                 
            vPrintString("        ===== = `-(_)--(_)-'     ||      ||                    ||||   \n");
            vPrintString("                              |||||||||||||                   _||||_  \n");
           
                         
                   
                   
        
        
        }
        
        xStatus = xQueueSendToBack(xParkingQueue, &IDandEntryExitTime1, 0);//要插入的queue handle. 從哪裡來的data的址, 插不進去願意等多久
        if (xStatus) {

        }
        else {
            vPrintString("插入xParkingQueue失敗");
        }
        xSemaphoreGive(ParkingArea_1.xParkingMutex);

    }


};
static uint32_t ulBitsToDecimal(EventBits_t Bits) {
    int lShift_count = 0;
    while (Bits != 1) {
        Bits >>= 1;//基操
        lShift_count++;
    }
    return lShift_count;
};

static void prvTimerCallback(TimerHandle_t xTimer)
{
    TickType_t xTimeNow;
    uint32_t ulExecutionCount;
    BaseType_t xStatus,xStatus2;
    IDandEntryExitTime IDandEntryExitTimeReceive;
    char cString[100] = "";
    char cText[10];
    int car_count = 0;
    int loop_count = 0;
    //vPrintString("\n閘道器做事，要把所有離場的車都傳給伺服器\n");

    do {
        xStatus = xQueueReceive(xParkingQueue, &IDandEntryExitTimeReceive, xTicksToWait);//回傳是否收到 收不到最多等100ms

        if (xStatus == pdPASS) {//有收到!
            if (loop_count == 0) {
                if(!animationmode)
                vPrintString("\n閘道器做事(因為離場閘門啟動)，要把所有離場的車都傳給伺服器\n");
                else {
                    vPrintString("  _______              _______  \n");
                    vPrintString(" |       |  離場資訊  |       | \n");
                    vPrintString(" | 閘道器|   ----->   |伺服器 | \n");
                    vPrintString(" |_______|  傳送中    |_______| \n");
                
                }
            }
            strcpy(cString, "");
            sprintf(cText, "%d", IDandEntryExitTimeReceive.lCarID);
            strcat(cString, " Car ID: "); // 添加字串
            strcat(cString, cText);
            sprintf(cText, "        ");
            sprintf(cText, "%d", IDandEntryExitTimeReceive.xEntryTime);
            strcat(cString, " Entry Time: ");
            strcat(cString, cText);
            sprintf(cText, "        ");
            sprintf(cText, "%d", IDandEntryExitTimeReceive.xExitTime);
            strcat(cString, " Exit Time: ");
            strcat(cString, cText);
            strcat(cString, "\n");
            xStatus2=xMessageBufferSend(xParkingMessageBuffer, (void*)cString, sizeof(cString), 0);
            if (xStatus2) {
               // vPrintString("MessageBuffer send pass\n");
            }
            else {
                //vPrintString("MessageBuffer send fail\n");
            }
            loop_count = 2;
            car_count++;
        }
        else {
            if (loop_count == 0) {
                //  vPrintString("\n閘道器做事(因為離場閘門啟動)，要把所有離場的車都傳給伺服器\n");
            }
            if (car_count != 0) {
                //printf("總共%d台車離場, 已將全數資料傳給伺服器\n",car_count);
                //vPrintString("現在沒有車需要傳了\n");
                loop_count = 2;
            }
            else {
                // vPrintString("沒有車要離場\n");
                loop_count = 2;
            }

        }
    } while (xStatus == 1);

}
static void vServerTask(void* pvParameters) {
    char cString[100] = "";
    char* pcString;
    BaseType_t xStatus;
    for (;;) {
        // xStatus=xQueueReceive(xParkingMessageBuffer, &cString, 0);
        xStatus = xMessageBufferReceive(xParkingMessageBuffer, &cString, sizeof(cString), pdMS_TO_TICKS(100UL));
        if (xStatus) {
            if (!animationmode) {
                vPrintString("\n伺服器收到了閘道器給的離場資訊: ");
                vPrintString(cString);
            }
            else {
                vPrintString("     ________________________________________________________    \n");
                vPrintString("    |                     伺服器顯示資料                     |    \n");
                vPrintString("    |  ");
                    vPrintString(cString);
                  
                    vPrintString("    |________________________________________________________|    \n");
               
                vPrintString("                               ||||           \n");
                vPrintString("                               ||||           \n");
                vPrintString("                              _||||_          \n");

            
            }
        }
        else {
          
            vTaskDelay(pdMS_TO_TICKS(100UL));
        }
    }
};

