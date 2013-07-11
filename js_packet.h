#ifndef JS_PACKET_H
#define JS_PACKET_H

#define JOY_ID              0xFDA55ADF
#define JOY_NUMBER_AXES     6

typedef struct
{
    uint32_t joy_id;
    int16_t axis[JOY_NUMBER_AXES];
    uint32_t checksum;
} js_packet_t;



#endif // JS_PACKET_H
