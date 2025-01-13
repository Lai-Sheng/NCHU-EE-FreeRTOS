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
static uint32_t ulNextRand;//�U�@���H����

QueueHandle_t xParkingQueue;
QueueHandle_t xCarIDandTime_Queue;//for queue handle
const TickType_t xDelay300ms = pdMS_TO_TICKS(300UL);
/*-----------------------------------------------------------*/

TaskHandle_t xEntry_HandlerTask;
TaskHandle_t xTollingHandlerTask;


enum State { eAvailable, eOccupied };
typedef struct Parking_Lot_t { //�C�氱��������F �{�b���A�B���P�B�i���ɶ�
    enum State eState;
    uint32_t ulCarID;
    TickType_t xEntryTime;
}xPark_Lot;

typedef struct Parking_Area {//�@�Ӱ�����
    xPark_Lot xLots[24]; //��24�Ӱ�����
    SemaphoreHandle_t xParkingMutex;//�Ψӱ���xLots�������s���A�T�O���|���H���٨S���}�A�N�Q���i�h
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
    BaseType_t xTimer1Started, xTimer2Started;//�ݬݦ��S���Ыئ��\
    vPrintString("�Ǹ�:0112209178 �@��:��|��\n");
    vPrintString("�w��Ө찱�����޲z�t�ΡA�п�ܾާ@�Ҧ�\n");
    vPrintString("��J0�A�����q��ܼҦ�\n");
    vPrintString("��J1�A��������ܼҦ�\n");
    scanf_s("%d",&animationmode);


    xProxyTimer = xTimerCreate("AutoReload Timer",
        mainAUTO_RELOAD_TIMER_PERIOD,
        pdTRUE,
        /* �p�ɾ��� ID ��l�Ȭ� 0�C */
        0,
        /* �ϥ� prvTimerCallback() �@���p�ɾ����^�I�禡 */
        prvTimerCallback);
    ParkingArea_1.xParkingMutex = xSemaphoreCreateMutex();

    // xParkingMessageBuffer = xQueueCreate(24, sizeof(char[100]));h
    xParkingMessageBuffer = xMessageBufferCreate(sizeof(char)*100*5);
    if (xParkingMessageBuffer) {
        //vPrintString("MessaeBuffer�Ыئ��\\n");
    }
    else {
        vPrintString("MessaeBuffer�Ыإ���\n");
    }

    xCarIDandTime_Queue = xQueueCreate(5, sizeof(IDandTime));
    xParkingQueue = xQueueCreate(5, sizeof(IDandEntryExitTime));
    xEventGroup = xEventGroupCreate();
   // xTaskCreate(vTollMachineWarmupTask, "TollMachineWarmup", 1000, NULL, 3, NULL);
    xTaskCreate(vPeriodicTaskEntry, "Periodic Interrupt Generator", 1000, NULL, 1, NULL);//�w����X���_ (�z�Lblock�ۤv)
    xTaskCreate(vPeriodicTaskTolling, "Periodic Tolling ", 1000, NULL, 1, NULL); //�w���q��TollingHandler(�z�Lblock�ۤv)


    xTaskCreate(vEntry_Handler, "Handle Notification Receiver", 1000, NULL, 2, &xEntry_HandlerTask);

    xTaskCreate(vTollingHandler, "Tolling Receiver", 1000, NULL, 2, &xTollingHandlerTask);//TollingHandler����q���~����
    //�o�ӥ��ȨS����q���|block�ۤv��maxtime

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
            printf("�U���s���}�i�Ӫ����j�@��: %d\n", xInterruptFrequency*10);
            vTaskDelay(xInterruptFrequency);//����@�U
        }
        isFirstTime = 0;
        // vPrintString("Periodic task - About to generate an interrupt.\r\n");
        vPortGenerateSimulatedInterrupt(mainINTERRUPT_NUMBER);//��X���_! (�|���������ulExampleInterruptHandler���_�B�z�{��)
        //vPrintString("Periodic task - Interrupt generated.\r\n\r\n\r\n");
    }

};
static void vPeriodicTaskTolling(void* pvParameters) {
    int isFirstTime = 1;
    if(!animationmode)
    vPrintString("ú�O���������A����10��\n");
    else {
        vPrintString("=====================\n");
        vPrintString("|  ú�O�������Ұʤ� |\n");
        vPrintString("=====================\n");
        vPrintString("|     _________     |\n");
        vPrintString("|    |         |    |\n");
        vPrintString("|    |_________|    |\n");

        vPrintString("|   [ ������... ]   |\n");

        vPrintString("|   [ �е���10��]   |\n");
        vPrintString("=====================\n");
    }
    vTaskDelay(pdMS_TO_TICKS(10000UL));//����@�U
    int lOccup_index;
    for (;; )
    {
        if (!animationmode) {
            if (isFirstTime) vPrintString("10���F�Aú�O�����������A�}�l�Ұ�\n");
        }
        else {
            if (isFirstTime) {
                vPrintString("=====================\n");
                vPrintString("|  ú�O����������    |\n");
                vPrintString("=====================\n");
                vPrintString("|     _________     |\n");
                vPrintString("|    |         |    |\n");
                vPrintString("|    |_________|    |\n");

                vPrintString("|   [ �Ұʦ��\ ]   |\n");

                vPrintString("|   [ 10��w����]   |\n");
                vPrintString("=====================\n");
            }
        }


        TickType_t xInterruptFrequency = pdMS_TO_TICKS(prvRand() % 2000 + 1000);
        if (!isFirstTime)
            vTaskDelay(xInterruptFrequency);//����@�U
        isFirstTime = 0;

        //if (xSemaphoreGive(ParkingArea_1.xParkingMutex))//��o�i�i�H���q����(��ܸ귽�S��)
        xSemaphoreTake(ParkingArea_1.xParkingMutex, portMAX_DELAY);
        //�D�@�ӪŦ�⨮���i�h 
        lOccup_index = lrdm_xLots_Occup_index(ParkingArea_1.xLots);
        if (!animationmode) {
            vPrintString("\nú�O������(���Hú�O)\n");
           // vPrintString("\n  Tolling notify and send Lots Number to handler\r\n");
        }
        else {
            vPrintString("      ________          \n");
            vPrintString("     |        |    ��\n");
            vPrintString("     | ú�O�� |   /�x\\\n");
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
            //���M�����쪺�q���Ȫ������
            if (!animationmode) {
               // vPrintStringAndNumber("  Receive Lots Number: ", lreceive_index);
                //vPrintStringAndNumber("  ���� Ocuppied: ", lreceive_index);
               // vPrintString("\n");
            }
            xEventGroupClearBits(xEventGroup, 0xFFFFFF);//�M��24��
            xEventGroupSetBits(xEventGroup, (1UL << lreceive_index));
           // vPrintString("  Updating global ParkingEventGroup\n");
            ParkingArea_1.xParkingEventGroup = xEventGroupGetBits(xEventGroup);
            //vPrintStringAndNumber("�{�bEvent Group����: ", ParkingArea_1.xParkingEventGroup);
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
    //�n���J��queue handle. �q���̨Ӫ�data���}, �����i�h�@�N���h�[
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
        ulEventsToProcess = ulTaskNotifyTake(pdTRUE, xMaxExpectedBlockTime);//�i�H���D�o�ӥ��Ȧ���q���F�S
        //�p�G����q��ulEventsToProcessc�|++(�i�H���D����X���q��)
        if (ulEventsToProcess != 0)
        {
            if (uxQueueMessagesWaiting(xCarIDandTime_Queue) != 0)//�d���X��item�bqueue�̭�
            {
                //  vPrintString("Queue should have been empty!\r\n");
            }
            while (ulEventsToProcess > 0)
            {
                xStatus = xQueueReceive(xCarIDandTime_Queue, &IDandTimeReceive, xTicksToWait);//�^�ǬO�_���� ������̦h��100ms

                if (xStatus == pdPASS)//������!
                {
                    lReceivedCarID = IDandTimeReceive.rdmCarID;
                    xReceivedTime = IDandTimeReceive.xTimeNow;
                    xSemaphoreTake(ParkingArea_1.xParkingMutex, portMAX_DELAY); //�D�@�ӪŦ�⨮���i�h
                    llots_index = lrdm_xLots_Avail_index(ParkingArea_1.xLots);
                    ParkingArea_1.xLots[llots_index].ulCarID = lReceivedCarID;
                    ParkingArea_1.xLots[llots_index].xEntryTime = xReceivedTime;
                    if (!animationmode) {
                        vPrintString("\n�i���h������(�C1~3�����i��)\n");
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
                        printf("            ��||   �h ||          ____                 |Entry Time: %d      \n", ParkingArea_1.xLots[llots_index].xEntryTime);
                        
                        vPrintString("_______     ��||======||     .__.'/||_|\\               |_________________|    \n");
                        vPrintString("|������|    �F||   �� ||    /_");
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
                ulEventsToProcess--;//(����⦬�쪺�q���ƶq���ӧ�)
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

    /*�Ω�ͦ����H���ƪ���Ψ�ơA�]�� MSVC rand() ��ƨ㦳�N�Q���쪺��G�C*/
    taskENTER_CRITICAL();
    ulNextRand = (ulMultiplier * ulNextRand) + ulIncrement;
    ulReturn = (ulNextRand >> 16UL) & 0x7fffUL;
    taskEXIT_CRITICAL();
    return ulReturn;
}

static void prvSRand(uint32_t ulSeed)
{
    /*�����H���ƥͦ������Ѻؤl����Ψ�ơC*/
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
    const EventBits_t xBitsToWaitFor = 0xffffff;//24�줸�Ҭ�1
    // EventBits_t xEventGroupValue;//�o�ӭȷ|�C�@����s�A�����F�{�b��bit���A
    int32_t lExit_index;

    IDandEntryExitTime IDandEntryExitTime1;
    BaseType_t xStatus;
    for (;; )
    {
       
        ParkingArea_1.xParkingEventGroup = xEventGroupWaitBits(
            xEventGroup,//Ū���ƥ��
            xBitsToWaitFor,//�Ʊ榳�H�ŦX�o��
            pdFALSE,//pdTRUE�G���������A�N uxBitsToWaitFor ������M�s�C
            pdFALSE,//true :uxBitsToWaitFor���O1����bxEventGroup���n1  false : uxBitsToWaitFor���O1����bxEventGroup���@��1�N�i�H
            portMAX_DELAY);
        //�W���ո����A���DxEventGroupn���ݪ�bit�ŦXxBitsToWaitFor�A���M�|�o��task�|�@���d�b�o��!block

        //�p�G�d�� ��Ƥ��|�ߧY��^�A�ҥH�S���^�ǭ�
        //�Y�O�q�L�A�i�H�i�@�B�P�_�O�]�����@�Ӧ줸�q�L �ȬOxEventGroup���ݪ���
        //xEventGroupWaitBits�o��api�|�@���b�I�������B�@
        lExit_index = ParkingArea_1.xParkingEventGroup;
        xEventGroupClearBits(xEventGroup, 0xFFFFFF);//�M��24��
        ParkingArea_1.xParkingEventGroup = xEventGroupGetBits(xEventGroup);
        lExit_index = ulBitsToDecimal(lExit_index);
       
        xSemaphoreTake(ParkingArea_1.xParkingMutex, portMAX_DELAY); 
        IDandEntryExitTime1.lCarID = ParkingArea_1.xLots[lExit_index].ulCarID;
        IDandEntryExitTime1.xEntryTime = ParkingArea_1.xLots[lExit_index].xEntryTime;
        IDandEntryExitTime1.xExitTime = xTaskGetTickCount();
        if (!animationmode) {
            vPrintString("\n�����h������(�]�����Hú�O)\n");
           // vPrintString("Car is exiting!\n");
           // vPrintStringAndNumber("Lots_number: ", lExit_index);
        }
        else {
            vPrintString("                              |||||||||||||             __________________     \n");
            vPrintString("                ____             ||      ||            |                 |\n");
            vPrintString("_______       /|_ || \\`.__       ||      ||��          ");
            printf("|Exit Time: %d      \n", IDandEntryExitTime1.xExitTime);

            vPrintString("|������|      (_    _ _    \\     ||======||��          |_________________| \n");
                  printf("|  %d  |     (_ _  _  %d \\     ||      ||�F                  ||||  \n", lExit_index, ParkingArea_1.xLots[lExit_index].ulCarID);
                 
            vPrintString("        ===== = `-(_)--(_)-'     ||      ||                    ||||   \n");
            vPrintString("                              |||||||||||||                   _||||_  \n");
           
                         
                   
                   
        
        
        }
        
        xStatus = xQueueSendToBack(xParkingQueue, &IDandEntryExitTime1, 0);//�n���J��queue handle. �q���̨Ӫ�data���}, �����i�h�@�N���h�[
        if (xStatus) {

        }
        else {
            vPrintString("���JxParkingQueue����");
        }
        xSemaphoreGive(ParkingArea_1.xParkingMutex);

    }


};
static uint32_t ulBitsToDecimal(EventBits_t Bits) {
    int lShift_count = 0;
    while (Bits != 1) {
        Bits >>= 1;//���
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
    //vPrintString("\n�h�D�����ơA�n��Ҧ������������ǵ����A��\n");

    do {
        xStatus = xQueueReceive(xParkingQueue, &IDandEntryExitTimeReceive, xTicksToWait);//�^�ǬO�_���� ������̦h��100ms

        if (xStatus == pdPASS) {//������!
            if (loop_count == 0) {
                if(!animationmode)
                vPrintString("\n�h�D������(�]�������h���Ұ�)�A�n��Ҧ������������ǵ����A��\n");
                else {
                    vPrintString("  _______              _______  \n");
                    vPrintString(" |       |  ������T  |       | \n");
                    vPrintString(" | �h�D��|   ----->   |���A�� | \n");
                    vPrintString(" |_______|  �ǰe��    |_______| \n");
                
                }
            }
            strcpy(cString, "");
            sprintf(cText, "%d", IDandEntryExitTimeReceive.lCarID);
            strcat(cString, " Car ID: "); // �K�[�r��
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
                //  vPrintString("\n�h�D������(�]�������h���Ұ�)�A�n��Ҧ������������ǵ����A��\n");
            }
            if (car_count != 0) {
                //printf("�`�@%d�x������, �w�N���Ƹ�ƶǵ����A��\n",car_count);
                //vPrintString("�{�b�S�����ݭn�ǤF\n");
                loop_count = 2;
            }
            else {
                // vPrintString("�S�����n����\n");
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
                vPrintString("\n���A������F�h�D������������T: ");
                vPrintString(cString);
            }
            else {
                vPrintString("     ________________________________________________________    \n");
                vPrintString("    |                     ���A����ܸ��                     |    \n");
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

