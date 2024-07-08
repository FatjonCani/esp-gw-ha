#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <regex.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "lwip/netdb.h"
#include "esp_http_client.h"
#include "mqtt_client.h"
//#include ""

//#include <cJSON.h>
static EventGroupHandle_t wifi_event_group;
uint8_t published=0;
static EventGroupHandle_t mqtt_event_group;
SemaphoreHandle_t xMutex;
SemaphoreHandle_t xMutexScanConnect=NULL;
static QueueHandle_t queue;
esp_netif_t* esp_netif_sta = NULL;
esp_netif_t* esp_netif_ap  = NULL;
// QueueHandle_t queueAddress;
// QueueHandle_t device_nr;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

double *outside_temp=NULL;//default value
esp_mqtt_client_handle_t client = NULL;




#define WIFI_AP_IP					"192.168.0.1"		// AP default IP
#define WIFI_AP_GATEWAY				"192.168.0.1"		// AP default Gateway (should be the same as the IP)
#define WIFI_AP_NETMASK				"255.255.255.0"	



EventGroupHandle_t wifi_event;

#define RETRY_THRESHOLD 5

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAILED_BIT     BIT1

#define MQTT_CONNECTED_BIT BIT0

uint8_t retry_num=0;


typedef struct vendor_data {
 float temp;
 //u shtua e re
 char descriptor[20];
 //u shtua e re
 }vendor_data;
// #define ESP_WIFI_SSID      "80211"


typedef struct element {
    uint8_t mac[6];
    float temp;
    
    
    char descriptor[20]; //u shtua me 21/06
}element;
 element* baza;
    enum flag {
        INIT_VALUE=0,
        NOT_FOUND,
        FOUND
       };

static const char *TAG = "wifi STA";

//element* baza;
void connectSta();
uint8_t* number1=NULL;;
//uint8_t* number1; ->punoi
static void skano();
   
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if(event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_DISCONNECTED    :
        if( retry_num < RETRY_THRESHOLD ){
          //  connect_init=2;//duhet ta bejme enum qe te dallojm kur kemi disconnect nga unstable network dhe kur duam ne ti bejm disconnect
            esp_wifi_connect();
            retry_num++;
        }else{
            xEventGroupSetBits(wifi_event_group,WIFI_FAIL_BIT);

        }
        
            
            break;
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            ESP_LOGI("TRYING TO","connect ,1st try");
         //   connect_init=1;
            break;
        default:
         ESP_LOGI("OTHER","ESP_EVENT_ID THAN IP_EVENT_STA_CONNECTED DHE DISCONNECTED");
            break;
        }

    }
    else if(event_base == IP_EVENT){
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t* ip_event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI("EVENT IP","got IP" IPSTR, IP2STR(&ip_event->ip_info.ip));
            ESP_LOGI("EVENT IP","got IP" "gIVE IT A TRY");
            xEventGroupSetBits(wifi_event_group,WIFI_CONNECTED_BIT);
            retry_num=0;
            break;
        
        default:
        ESP_LOGI("OTHER","ESP_EVENT_ID THAN IP_EVENT_sTA_GOT_IP");
            break;
        }
    }else {
        ESP_LOGI("OTHER EVENT","BASE");
    }
}



static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    //ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG,"MQTT EVENT BEFORE CONNECT");
            break;
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        xEventGroupSetBits(mqtt_event_group,MQTT_CONNECTED_BIT);

       // MQTT_CONNEECTED=1;

        vTaskDelay(8000 / portTICK_PERIOD_MS);
        // esp_mqtt_client_publish(client, "sensor/temp", "{\"temp\":13}", 0, 2, 0);
        //ktu bejme skanimin dhe bejme auto discovery
        //dhe bejme publish per secilen
        //shtojm tek event group 1 bit qe e perdor me von tek publisher task
        
        //msg_id = esp_mqtt_client_subscribe(client, "/topic/test1", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=");

         //esp_mqtt_client_publish(client, "/topic/test3", "Helllo World", 0, 0, 0);
        // msg_id = esp_mqtt_client_subscribe(client, "/topic/test2", 1);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        esp_mqtt_client_reconnect(client);
       // esp_mqtt_client_start(client);
        //MQTT_CONNEECTED=0;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        
        // ESP_LOGI("EVENT DATA","TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // ESP_LOGI("EVENT DATA","DATA=%.*s\r\n", event->data_len, event->data);
        //     value= regcomp(&regex,topic_r1,0);
        //     if(value==0){
        //         ESP_LOGI("INITIALIZED","OK");
        //     }else{
        //         ESP_LOGI("INITIALIZED","ERROR");
        //     }
        // if(regexec(&regex,event->topic,0,NULL,0)==0){
        //     ESP_LOGI("lEXUAM DATA","NGA TOPIC QE DONIM,tani lexo event->data me kuptu on or off");
        //     value=regcomp(&regex,pergjigja2,0);
        //        if(value==0){
        //         ESP_LOGI("SECOND INITIALIZED","OK");
        //     }else{
        //         ESP_LOGI("SECOND INITIALIZED","ERROR");
        //     }
        //     if(regexec(&regex,event->data,0,NULL,0)==0)
        //     {
        //         ESP_LOGI("DATA qe erdhi eshte : data","sakt");
        //         ESP_LOGI("SAKT","SAKT");
        //     }else{
        //         ESP_LOGI("SOMETHING","WENT WRONG");
        //     }
        //     /*
        //     regex event->data_len
        //     */
        //   value= esp_mqtt_client_unsubscribe(client,event->topic);
        //   if(value!=-1){
        //     ESP_LOGI("unsubscribed","UNSUBSCRIBED");//technically duhet me i ba subscribe cdo her
        //     //para se me shkrujt
        //   }
        // }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void wifi_promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    // if (type != WIFI_PKT_MGMT){
    //     ESP_LOGI(TAG,"No management data");
    //  return; // Only process management packets
    // }
   //  ESP_LOGI("KALOI RX","E KALOI");
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *payload = ppkt->payload;

    // Check if it's an action frame

    if ((payload[0] == 0xD0) && (payload[24] == 0x7F) && (payload[27]==0x0C) ) {
        // Extract the source address, OUI, and vendor-specific content
        
        
        uint8_t src_mac[6];
        memcpy(src_mac, payload + 10, 6);
       
        uint8_t oui[3];
        memcpy(oui, payload + 26, 3);
       
        // uint8_t vendor_data1[20];  beji uncomment ksaj 21/06
        //u shtua e re
        uint8_t vendor_data1[24];
        // u shtua e re

        int ab=sizeof(vendor_data);
        memcpy(vendor_data1, payload + 29, ab);
        vendor_data* a;
        a=(vendor_data*)vendor_data1;
       ESP_LOGI("Descriptioni",":%s",a->descriptor);
   
      enum flag flag_1=INIT_VALUE;
     
//              TEST

    element b;
    b.temp=a->temp;
    b.mac[0]=src_mac[0];
    b.mac[1]=src_mac[1];
    b.mac[2]=src_mac[2];
    b.mac[3]=src_mac[3];
    b.mac[4]=src_mac[4];
    b.mac[5]=src_mac[5];
    memcpy(b.descriptor,a->descriptor,20); // u shtua me 21/06

    xQueueSend(queue,&b,portMAX_DELAY);
        //-------------------------------------------------
        ESP_LOGI(TAG, "Received vendor-specific action frame from %02x:%02x:%02x:%02x:%02x:%02x",
                 src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
        ESP_LOGI(TAG, "OUI: %02x:%02x:%02x", oui[0], oui[1], oui[2]);
        ESP_LOGI(TAG, "Vendor data: %f", a->temp); //
    }}


static void mqtt_app_start(void)
{

    EventBits_t bit= xEventGroupWaitBits(wifi_event_group,WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE,pdFALSE,portMAX_DELAY);

    if(bit & WIFI_CONNECTED_BIT){
    ESP_LOGI(TAG, "STARTING MQTT");
    esp_mqtt_client_config_t mqttConfig = {
        .broker.address.uri="mqtt://10.0.10.124:1883",
        .credentials.username="mosquitto",
        .credentials.authentication.password="fc3fc3"
        //.uri = "mqtt://10.0.10.33:1883"
        };
    
    client = esp_mqtt_client_init(&mqttConfig);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    //vTaskDelay(8000/portTICK_PERIOD_MS); // u shtua per te leju connectionin 


    // EventBits_t bit_mqtt = xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
     
    // if(bit_mqtt & MQTT_CONNECTED_BIT){
    //  esp_wifi_set_promiscuous(true);
    //  esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_rx_cb);
    // }else
    // {
    //     ESP_LOGI("MQTT","NOT CONNECTED");
    //     //i guess do beja 1 esp_restart
    // }
    }
    else if(bit & WIFI_FAIL_BIT){
        ESP_LOGI("FAILED"," TO CONNECT EVEN THOUGH IT RETRIED 5 TIMES");
    }else{
        ESP_LOGI("UNEXPECTED","BEHAVIOUR");
    }
}




void wifi_init_sta(void)
{

     wifi_event_group= xEventGroupCreate();
     mqtt_event_group= xEventGroupCreate();
    outside_temp=(double*)malloc(sizeof(double));
    *outside_temp=-1000;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
   esp_netif_sta= esp_netif_create_default_wifi_sta();
/*
-----------prov
*/
   esp_netif_ap= esp_netif_create_default_wifi_ap();
/*
---------prov
*/
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,wifi_event_handler,NULL));
     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
   
       wifi_config_t wifi_config2 = {
        .ap={
            .ssid="KINETON_GATEWAY",
            .authmode=WIFI_AUTH_OPEN,
            .max_connection=50,

        }
    };
      wifi_config_t wifi_config = {
         .sta = {
             .ssid="Kine Lab",
            .password="K1n3t0n4343"
            
         }};
      
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config2));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  // connectSta();

	esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));

	esp_netif_dhcps_stop(esp_netif_ap);					///> must call this first
	inet_pton(AF_INET, WIFI_AP_IP, &ap_ip_info.ip);		///> Assign access point's static IP, GW, and netmask
	inet_pton(AF_INET, WIFI_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, WIFI_AP_NETMASK, &ap_ip_info.netmask);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));			///> Statically configure the network interface
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));						///> Start the AP DHCP server (for connecting stations e.g. your mobile device)


    ESP_ERROR_CHECK(esp_wifi_start());
    
 

    // esp_wifi_set_promiscuous(true);
    // esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_rx_cb);
    
    ESP_LOGI(TAG, "wifi_init_sta finished.");
   // queueAddress=xQueueCreate(10,sizeof(element));
    mqtt_app_start();

}





// char* KinetonName ="ESP_Kineton_Device";
char* KinetonName="80211";

static void skano(){
  char data[150]="\0";
  char topic[40]="\0";


  EventBits_t bit_mqtt=xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
  if(bit_mqtt & MQTT_CONNECTED_BIT){
     ESP_LOGI("Skano:","Entered");
    esp_err_t err = esp_wifi_scan_start(NULL,true);
    if(err!=ESP_OK){
        ESP_LOGI("ERROR","SKANIMI NUK ECEN");
    }
    uint16_t scanned_ap=20;
    regex_t regex;
    if(number1==NULL){
    number1=(uint8_t*)malloc(sizeof(uint8_t));}
    memset(number1,0,sizeof(uint8_t));
   
    wifi_ap_record_t ap_record[20];
    memset(ap_record,0,sizeof(ap_record));
    err = esp_wifi_scan_get_ap_records(&scanned_ap,ap_record);
     esp_wifi_scan_get_ap_num(&scanned_ap);
    uint8_t compare=regcomp(&regex,KinetonName,0);
    baza=(element*)malloc(sizeof(element));
     memset(baza,0x00,sizeof(element));
     uint8_t nr=0;
    //test
    ESP_LOGI("TEST","Scanned ap number:%d",scanned_ap);
    
     for(uint8_t i =0; i<20 ; i++){
        ESP_LOGI("TEST","SSID:%s",ap_record[i].ssid);
         compare=regexec(&regex,(char*)ap_record[i].ssid,0,NULL,0);
        if(compare==0){
            sprintf(topic,"homeassistant/sensor/temp%u/config",nr);
            ESP_LOGI("Topic ku po ben publish auto discovery:","%s",topic);
          sprintf(data,"{\"name\" : \"Temp%u\",\"state_topic\": \"sensor/temp%u\",\"value_template\": \"Temp:{{value_json.temp}} - Descr:{{value_json.Description}}\"}",nr,nr);
            esp_mqtt_client_publish(client,topic,data,0,2,0);
             ESP_LOGI("Topic ku po ben publish auto discovery:","%s",data);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            //memset(topic)
            
            //
            //i guess ketu posht pres 1 event bit ose 1 variabel nga mqtt_client_published
            
             for(uint8_t m=0;m<6 ; m++){
               
             (baza+nr)->mac[m]=ap_record[i].bssid[m];
       
             }
             (baza+nr)->temp=0;
            ESP_LOGI("ISHALLA","SAKT %d  tjeter",scanned_ap);
            baza=(element*)realloc(baza,sizeof(element)*(nr+2));
           
           nr++;
           
            
            


}}
(*number1)=nr;
 
 //xQueueSend(device_nr,&nr,portMAX_DELAY);

 //ketu tek skano krijojm keto entitet nepermjet mqtt discovery

        ESP_LOGI("ACTIVATED","PROMISCUOUS");
     ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
     ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_rx_cb));
  


}}

static void mes_func(void* param){
    //element* baza=skano();
   //  connectSta();
    skano();
 //connectSta();
 char topic[40]="\0";
 char data[80]="\0";
    element m;
    if(baza!=NULL){
        ESP_LOGI("TASK_TJETER","not null");
    }else{
        ESP_LOGI("TASK_TJETER","BAZA NUK ESHTE INICIALIZU");
    }
    if(number1!=NULL){
        ESP_LOGI("Edhe numri","nuk eshte null");
    }else{
        ESP_LOGI("EDHE numri"," eshte null");
    }

    for(;;){
     
        if(xQueueReceive(queue,&m,portMAX_DELAY)){
               if(xSemaphoreTake(xMutex,portMAX_DELAY)){

            ESP_LOGI("TAG","U fute teK tasKU MAC:%x",m.mac[0]);

        enum flag flag_1=INIT_VALUE;
        // Log or process the received data

  
//        //-------------------------- TEST
      for(uint8_t itr=0;itr<*number1;itr++){
            flag_1=INIT_VALUE;
            for(uint8_t i=0;i<6;i++){
//                  //vTaskDelay(1000 / portTICK_PERIOD_MS);
               if(m.mac[i]!=(baza+itr)->mac[i]){
                 flag_1=NOT_FOUND;
            ESP_LOGI("NOT FOUND","NOT FOUND");
                     break;
        
            }
             }
           if(flag_1==NOT_FOUND){
                continue;
            }
            flag_1=FOUND;

        /*
        ktu ben publish
        */
    sprintf(topic,"sensor/temp%u",itr);
    sprintf(data,"{\"temp\":\"%f\",\"Description\":\"%s\"}",(baza+itr)->temp,(baza+itr)->descriptor);
    esp_mqtt_client_publish(client,topic,data,0,2,0);
           (baza+itr)->temp=m.temp;//kapim itr qe qe me percaktu cili node eshte i guess
           memcpy((baza+itr)->descriptor,m.descriptor,20);//u shtua me 21/06 duhesh edhe me i ba memset 0 char kur sapo e inicializojm ose non init to all
            // ESP_LOGI(TAG,"Value found from the struct array , temp:%f",(baza+itr)->temp);
           break;
         }
 float mes=0;
  //xQueueReset(queueAddress);
        for(uint8_t i =0;i<*number1;i++){
             mes+=(baza+i)->temp;
             // xQueueSendFromISR(queueAddress,&(baza+i)->temp,portMAX_DELAY);
    
      }
    
       mes=mes/(*number1);




    xSemaphoreGive(xMutex);


        }



     }
}


}


void app_main(void)
{
    //Initialize NVS
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
  //  xMutexScanConnect=xSemaphoreCreateBinary();
    xMutex=xSemaphoreCreateMutex();
 

   //  skano();
    //  queueAddress=xQueueCreate(1,sizeof(uint32_t));
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    queue=xQueueCreate(4,sizeof(element));
    
    wifi_event = xEventGroupCreate();
    wifi_init_sta();
    //queue=xQueueCreate(4,sizeof(element));
   
    xTaskCreatePinnedToCore(mes_func,"mes_func",8192,NULL,4,NULL,1);
    
    //xTaskCreatePinnedToCore(get_outside_temp,"get_outside_temp",1024,NULL,4,NULL,1);
    // wifi_init_sta();
    
}
