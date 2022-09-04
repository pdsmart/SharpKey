/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            BTHID.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header file for the Bluetooth Keyboard Class.
//
// Credits:         
// Copyright:       (c) 2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
//                  Jun 2022 - Updated with latest findings. Now checks the bonded list and opens 
//                             connections or scans for new devices if no connections exist. 
//
// Notes:           See Makefile to enable/disable conditional components
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// This source file is free software: you can redistribute it and#or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This source file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef BT_KEYBOARD_H_
#define BT_KEYBOARD_H_

#include <string>
#include <vector>
#include <cstring>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_hidh.h"
#include "esp_hid_common.h"
#include "esp_gap_bt_api.h"
#include "esp_gap_ble_api.h"
#include "PS2KeyAdvanced.h"
#include "PS2Mouse.h"
#include "BT.h"

// Keyboard is a sub-class of BT which provides methods to setup BT for use by a keyboard.
class BTHID : public BT  {
    // Macros.
    //
    #define NUMELEM(a)                     (sizeof(a)/sizeof(a[0]))

    // Global constants
    #define MAX_KEYBOARD_DATA_BYTES        8 
    #define MAX_CCONTROL_DATA_BYTES        3 
    #define MAX_MOUSE_DATA_BYTES           7
    #define MAX_BT2PS2_MAP_ENTRIES         179 
    #define MAX_BTMEDIA2PS2_MAP_ENTRIES    8

    // LED's
    #define BT_LED_NUMLOCK                 0x01
    #define BT_LED_CAPSLOCK                0x02
    #define BT_LED_SCROLLLOCK              0x04

    // Control keys.
    #define BT_NONE                        0x0000
    #define BT_CTRL_LEFT                   0x0001
    #define BT_SHIFT_LEFT                  0x0002
    #define BT_ALT_LEFT                    0x0004
    #define BT_GUI_LEFT                    0x0008
    #define BT_CTRL_RIGHT                  0x0010
    #define BT_SHIFT_RIGHT                 0x0020
    #define BT_ALT_RIGHT                   0x0040
    #define BT_GUI_RIGHT                   0x0080
    #define BT_CAPS_LOCK                   0x0100
    #define BT_NUM_LOCK                    0x0200
    #define BT_SCROLL_LOCK                 0x0400
    #define BT_DUPLICATE                   0xFFFF                                  // Duplicate BT flags onto PS/2 flags.

    #define BT_PS2_FUNCTION                0x01
    #define BT_PS2_GUI                     0x02
    #define BT_PS2_ALT_GR                  0x04
    #define BT_PS2_ALT                     0x08
    #define BT_PS2_CAPS                    0x10
    #define BT_PS2_CTRL                    0x20
    #define BT_PS2_SHIFT                   0x40
    #define BT_PS2_BREAK                   0x80

    #define BT_KEY_NONE                    0x00                                    // No key pressed
    #define BT_KEY_ERR_OVF                 0x01                                    // Keyboard Error Roll Over
                                        // 0x02                                    // Keyboard POST Fail
                                        // 0x03                                    // Keyboard Error Undefined
    #define BT_KEY_A                       0x04                                    // Keyboard a and A
    #define BT_KEY_B                       0x05                                    // Keyboard b and B
    #define BT_KEY_C                       0x06                                    // Keyboard c and C
    #define BT_KEY_D                       0x07                                    // Keyboard d and D
    #define BT_KEY_E                       0x08                                    // Keyboard e and E
    #define BT_KEY_F                       0x09                                    // Keyboard f and F
    #define BT_KEY_G                       0x0a                                    // Keyboard g and G
    #define BT_KEY_H                       0x0b                                    // Keyboard h and H
    #define BT_KEY_I                       0x0c                                    // Keyboard i and I
    #define BT_KEY_J                       0x0d                                    // Keyboard j and J
    #define BT_KEY_K                       0x0e                                    // Keyboard k and K
    #define BT_KEY_L                       0x0f                                    // Keyboard l and L
    #define BT_KEY_M                       0x10                                    // Keyboard m and M
    #define BT_KEY_N                       0x11                                    // Keyboard n and N
    #define BT_KEY_O                       0x12                                    // Keyboard o and O
    #define BT_KEY_P                       0x13                                    // Keyboard p and P
    #define BT_KEY_Q                       0x14                                    // Keyboard q and Q
    #define BT_KEY_R                       0x15                                    // Keyboard r and R
    #define BT_KEY_S                       0x16                                    // Keyboard s and S
    #define BT_KEY_T                       0x17                                    // Keyboard t and T
    #define BT_KEY_U                       0x18                                    // Keyboard u and U
    #define BT_KEY_V                       0x19                                    // Keyboard v and V
    #define BT_KEY_W                       0x1a                                    // Keyboard w and W
    #define BT_KEY_X                       0x1b                                    // Keyboard x and X
    #define BT_KEY_Y                       0x1c                                    // Keyboard y and Y
    #define BT_KEY_Z                       0x1d                                    // Keyboard z and Z
    
    #define BT_KEY_1                       0x1e                                    // Keyboard 1 and !
    #define BT_KEY_2                       0x1f                                    // Keyboard 2 and @
    #define BT_KEY_3                       0x20                                    // Keyboard 3 and #
    #define BT_KEY_4                       0x21                                    // Keyboard 4 and $
    #define BT_KEY_5                       0x22                                    // Keyboard 5 and %
    #define BT_KEY_6                       0x23                                    // Keyboard 6 and ^
    #define BT_KEY_7                       0x24                                    // Keyboard 7 and &
    #define BT_KEY_8                       0x25                                    // Keyboard 8 and *
    #define BT_KEY_9                       0x26                                    // Keyboard 9 and (
    #define BT_KEY_0                       0x27                                    // Keyboard 0 and )
    
    #define BT_KEY_ENTER                   0x28                                    // Keyboard Return (ENTER)
    #define BT_KEY_ESC                     0x29                                    // Keyboard ESCAPE
    #define BT_KEY_BACKSPACE               0x2a                                    // Keyboard DELETE (Backspace)
    #define BT_KEY_TAB                     0x2b                                    // Keyboard Tab
    #define BT_KEY_SPACE                   0x2c                                    // Keyboard Spacebar
    #define BT_KEY_MINUS                   0x2d                                    // Keyboard - and _
    #define BT_KEY_EQUAL                   0x2e                                    // Keyboard = and +
    #define BT_KEY_LEFTBRACE               0x2f                                    // Keyboard [ and {
    #define BT_KEY_RIGHTBRACE              0x30                                    // Keyboard ] and }
    #define BT_KEY_BACKSLASH               0x31                                    // Keyboard \ and |
    #define BT_KEY_HASHTILDE               0x32                                    // Keyboard Non-US # and ~
    #define BT_KEY_SEMICOLON               0x33                                    // Keyboard ; and :
    #define BT_KEY_APOSTROPHE              0x34                                    // Keyboard ' and "
    #define BT_KEY_GRAVE                   0x35                                    // Keyboard ` and ~
    #define BT_KEY_COMMA                   0x36                                    // Keyboard , and <
    #define BT_KEY_DOT                     0x37                                    // Keyboard . and >
    #define BT_KEY_SLASH                   0x38                                    // Keyboard / and ?
    #define BT_KEY_CAPSLOCK                0x39                                    // Keyboard Caps Lock
    
    #define BT_KEY_F1                      0x3a                                    // Keyboard F1
    #define BT_KEY_F2                      0x3b                                    // Keyboard F2
    #define BT_KEY_F3                      0x3c                                    // Keyboard F3
    #define BT_KEY_F4                      0x3d                                    // Keyboard F4
    #define BT_KEY_F5                      0x3e                                    // Keyboard F5
    #define BT_KEY_F6                      0x3f                                    // Keyboard F6
    #define BT_KEY_F7                      0x40                                    // Keyboard F7
    #define BT_KEY_F8                      0x41                                    // Keyboard F8
    #define BT_KEY_F9                      0x42                                    // Keyboard F9
    #define BT_KEY_F10                     0x43                                    // Keyboard F10
    #define BT_KEY_F11                     0x44                                    // Keyboard F11
    #define BT_KEY_F12                     0x45                                    // Keyboard F12
    
    #define BT_KEY_SYSRQ                   0x46                                    // Keyboard Print Screen
    #define BT_KEY_SCROLLLOCK              0x47                                    // Keyboard Scroll Lock
    #define BT_KEY_PAUSE                   0x48                                    // Keyboard Pause
    #define BT_KEY_INSERT                  0x49                                    // Keyboard Insert
    #define BT_KEY_HOME                    0x4a                                    // Keyboard Home
    #define BT_KEY_PAGEUP                  0x4b                                    // Keyboard Page Up
    #define BT_KEY_DELETE                  0x4c                                    // Keyboard Delete Forward
    #define BT_KEY_END                     0x4d                                    // Keyboard End
    #define BT_KEY_PAGEDOWN                0x4e                                    // Keyboard Page Down
    #define BT_KEY_RIGHT                   0x4f                                    // Keyboard Right Arrow
    #define BT_KEY_LEFT                    0x50                                    // Keyboard Left Arrow
    #define BT_KEY_DOWN                    0x51                                    // Keyboard Down Arrow
    #define BT_KEY_UP                      0x52                                    // Keyboard Up Arrow
    
    #define BT_KEY_NUMLOCK                 0x53                                    // Keyboard Num Lock and Clear
    #define BT_KEY_KPSLASH                 0x54                                    // Keypad /
    #define BT_KEY_KPASTERISK              0x55                                    // Keypad *
    #define BT_KEY_KPMINUS                 0x56                                    // Keypad -
    #define BT_KEY_KPPLUS                  0x57                                    // Keypad +
    #define BT_KEY_KPENTER                 0x58                                    // Keypad ENTER
    #define BT_KEY_KP1                     0x59                                    // Keypad 1 and End
    #define BT_KEY_KP2                     0x5a                                    // Keypad 2 and Down Arrow
    #define BT_KEY_KP3                     0x5b                                    // Keypad 3 and PageDn
    #define BT_KEY_KP4                     0x5c                                    // Keypad 4 and Left Arrow
    #define BT_KEY_KP5                     0x5d                                    // Keypad 5
    #define BT_KEY_KP6                     0x5e                                    // Keypad 6 and Right Arrow
    #define BT_KEY_KP7                     0x5f                                    // Keypad 7 and Home
    #define BT_KEY_KP8                     0x60                                    // Keypad 8 and Up Arrow
    #define BT_KEY_KP9                     0x61                                    // Keypad 9 and Page Up
    #define BT_KEY_KP0                     0x62                                    // Keypad 0 and Insert
    #define BT_KEY_KPDOT                   0x63                                    // Keypad . and Delete
    
    #define BT_KEY_102ND                   0x64                                    // Keyboard Non-US \ and |
    #define BT_KEY_COMPOSE                 0x65                                    // Keyboard Application
    #define BT_KEY_POWER                   0x66                                    // Keyboard Power
    #define BT_KEY_KPEQUAL                 0x67                                    // Keypad =
    
    #define BT_KEY_F13                     0x68                                    // Keyboard F13
    #define BT_KEY_F14                     0x69                                    // Keyboard F14
    #define BT_KEY_F15                     0x6a                                    // Keyboard F15
    #define BT_KEY_F16                     0x6b                                    // Keyboard F16
    #define BT_KEY_F17                     0x6c                                    // Keyboard F17
    #define BT_KEY_F18                     0x6d                                    // Keyboard F18
    #define BT_KEY_F19                     0x6e                                    // Keyboard F19
    #define BT_KEY_F20                     0x6f                                    // Keyboard F20
    #define BT_KEY_F21                     0x70                                    // Keyboard F21
    #define BT_KEY_F22                     0x71                                    // Keyboard F22
    #define BT_KEY_F23                     0x72                                    // Keyboard F23
    #define BT_KEY_F24                     0x73                                    // Keyboard F24
    
    #define BT_KEY_OPEN                    0x74                                    // Keyboard Execute
    #define BT_KEY_HELP                    0x75                                    // Keyboard Help
    #define BT_KEY_PROPS                   0x76                                    // Keyboard Menu
    #define BT_KEY_FRONT                   0x77                                    // Keyboard Select
    #define BT_KEY_STOP                    0x78                                    // Keyboard Stop
    #define BT_KEY_AGAIN                   0x79                                    // Keyboard Again
    #define BT_KEY_UNDO                    0x7a                                    // Keyboard Undo
    #define BT_KEY_CUT                     0x7b                                    // Keyboard Cut
    #define BT_KEY_COPY                    0x7c                                    // Keyboard Copy
    #define BT_KEY_PASTE                   0x7d                                    // Keyboard Paste
    #define BT_KEY_FIND                    0x7e                                    // Keyboard Find
    #define BT_KEY_MUTE                    0x7f                                    // Keyboard Mute
    #define BT_KEY_VOLUMEUP                0x80                                    // Keyboard Volume Up
    #define BT_KEY_VOLUMEDOWN              0x81                                    // Keyboard Volume Down
                                                                                   //                            0x82  Keyboard Locking Caps Lock
                                                                                   //                            0x83  Keyboard Locking Num Lock
                                                                                   //                            0x84  Keyboard Locking Scroll Lock
    #define BT_KEY_KPCOMMA                 0x85                                    // Keypad Comma
                                                                                   //                            0x86  Keypad Equal Sign
    #define BT_KEY_RO                      0x87                                    // Keyboard International1
    #define BT_KEY_KATAKANAHIRAGANA        0x88                                    // Keyboard International2
    #define BT_KEY_YEN                     0x89                                    // Keyboard International3
    #define BT_KEY_HENKAN                  0x8a                                    // Keyboard International4
    #define BT_KEY_MUHENKAN                0x8b                                    // Keyboard International5
    #define BT_KEY_KPJPCOMMA               0x8c                                    // Keyboard International6
                                                                                   //                            0x8d  Keyboard International7
                                                                                   //                            0x8e  Keyboard International8
                                                                                   //                            0x8f  Keyboard International9
    #define BT_KEY_HANGEUL                 0x90                                    // Keyboard LANG1
    #define BT_KEY_HANJA                   0x91                                    // Keyboard LANG2
    #define BT_KEY_KATAKANA                0x92                                    // Keyboard LANG3
    #define BT_KEY_HIRAGANA                0x93                                    // Keyboard LANG4
    #define BT_KEY_ZENKAKUHANKAKU          0x94                                    // Keyboard LANG5
                                                                                   //                            0x95  Keyboard LANG6
                                                                                   //                            0x96  Keyboard LANG7
                                                                                   //                            0x97  Keyboard LANG8
                                                                                   //                            0x98  Keyboard LANG9
                                                                                   //                            0x99  Keyboard Alternate Erase
                                                                                   //                            0x9a  Keyboard SysReq/Attention
                                                                                   //                            0x9b  Keyboard Cancel
                                                                                   //                            0x9c  Keyboard Clear
                                                                                   //                            0x9d  Keyboard Prior
                                                                                   //                            0x9e  Keyboard Return
                                                                                   //                            0x9f  Keyboard Separator
                                                                                   //                            0xa0  Keyboard Out
                                                                                   //                            0xa1  Keyboard Oper
                                                                                   //                            0xa2  Keyboard Clear/Again
                                                                                   //                            0xa3  Keyboard CrSel/Props
                                                                                   //                            0xa4  Keyboard ExSel
    
                                                                                   //                            0xb0  Keypad 00
                                                                                   //                            0xb1  Keypad 000
                                                                                   //                            0xb2  Thousands Separator
                                                                                   //                            0xb3  Decimal Separator
                                                                                   //                            0xb4  Currency Unit
                                                                                   //                            0xb5  Currency Sub-unit
    #define BT_KEY_KPLEFTPAREN             0xb6                                    // Keypad (
    #define BT_KEY_KPRIGHTPAREN            0xb7                                    // Keypad )
                                                                                   //                            0xb8  Keypad {
                                                                                   //                            0xb9  Keypad }
                                                                                   //                            0xba  Keypad Tab
                                                                                   //                            0xbb  Keypad Backspace
                                                                                   //                            0xbc  Keypad A
                                                                                   //                            0xbd  Keypad B
                                                                                   //                            0xbe  Keypad C
                                                                                   //                            0xbf  Keypad D
                                                                                   //                            0xc0  Keypad E
                                                                                   //                            0xc1  Keypad F
                                                                                   //                            0xc2  Keypad XOR
                                                                                   //                            0xc3  Keypad ^
                                                                                   //                            0xc4  Keypad %
                                                                                   //                            0xc5  Keypad <
                                                                                   //                            0xc6  Keypad >
                                                                                   //                            0xc7  Keypad &
                                                                                   //                            0xc8  Keypad &&
                                                                                   //                            0xc9  Keypad |
                                                                                   //                            0xca  Keypad ||
                                                                                   //                            0xcb  Keypad :
                                                                                   //                            0xcc  Keypad #
                                                                                   //                            0xcd  Keypad Space
                                                                                   //                            0xce  Keypad @
                                                                                   //                            0xcf  Keypad !
                                                                                   //                            0xd0  Keypad Memory Store
                                                                                   //                            0xd1  Keypad Memory Recall
                                                                                   //                            0xd2  Keypad Memory Clear
                                                                                   //                            0xd3  Keypad Memory Add
                                                                                   //                            0xd4  Keypad Memory Subtract
                                                                                   //                            0xd5  Keypad Memory Multiply
                                                                                   //                            0xd6  Keypad Memory Divide
                                                                                   //                            0xd7  Keypad +/-
                                                                                   //                            0xd8  Keypad Clear
                                                                                   //                            0xd9  Keypad Clear Entry
                                                                                   //                            0xda  Keypad Binary
                                                                                   //                            0xdb  Keypad Octal
                                                                                   //                            0xdc  Keypad Decimal
                                                                                   //                            0xdd  Keypad Hexadecimal
    
    #define BT_KEY_LEFTCTRL                0xe0                                    // Keyboard Left Control
    #define BT_KEY_LEFTSHIFT               0xe1                                    // Keyboard Left Shift
    #define BT_KEY_LEFTALT                 0xe2                                    // Keyboard Left Alt
    #define BT_KEY_LEFTMETA                0xe3                                    // Keyboard Left GUI
    #define BT_KEY_RIGHTCTRL               0xe4                                    // Keyboard Right Control
    #define BT_KEY_RIGHTSHIFT              0xe5                                    // Keyboard Right Shift
    #define BT_KEY_RIGHTALT                0xe6                                    // Keyboard Right Alt
    #define BT_KEY_RIGHTMETA               0xe7                                    // Keyboard Right GUI
    
    #define BT_KEY_MEDIA_PLAYPAUSE         0xe8
    #define BT_KEY_MEDIA_STOPCD            0xe9
    #define BT_KEY_MEDIA_PREVIOUSSONG      0xea
    #define BT_KEY_MEDIA_NEXTSONG          0xeb
    #define BT_KEY_MEDIA_EJECTCD           0xec
    #define BT_KEY_MEDIA_VOLUMEUP          0xed
    #define BT_KEY_MEDIA_VOLUMEDOWN        0xee
    #define BT_KEY_MEDIA_MUTE              0xef
    #define BT_KEY_MEDIA_WWW               0xf0
    #define BT_KEY_MEDIA_BACK              0xf1
    #define BT_KEY_MEDIA_FORWARD           0xf2
    #define BT_KEY_MEDIA_STOP              0xf3
    #define BT_KEY_MEDIA_FIND              0xf4
    #define BT_KEY_MEDIA_SCROLLUP          0xf5
    #define BT_KEY_MEDIA_SCROLLDOWN        0xf6
    #define BT_KEY_MEDIA_EDIT              0xf7
    #define BT_KEY_MEDIA_SLEEP             0xf8
    #define BT_KEY_MEDIA_COFFEE            0xf9
    #define BT_KEY_MEDIA_REFRESH           0xfa
    #define BT_KEY_MEDIA_CALC              0xfb

    // Media key definition. On the ESP module a seperate usage type, CCONTROL is created for media keys and it delivers a 24bit word, each bit signifying a key.
    #define BT_MEDIA_SEARCH                0x00200000
    #define BT_MEDIA_HOME                  0x00080000
    #define BT_MEDIA_BRIGHTNESS_UP         0x00004000
    #define BT_MEDIA_BRIGHTNESS_DOWN       0x00008000
    #define BT_MEDIA_MUTE                  0x00000040
    #define BT_MEDIA_VOL_DOWN              0x00000020
    #define BT_MEDIA_VOL_UP                0x00000010
    #define BT_MEDIA_TRACK_PREV            0x00000001

    // PS2 Flag definitions.
    #define PS2_FLG_NONE                   0x00                                      // No keys active = 0
    #define PS2_FLG_SHIFT                  PS2_SHIFT    >> 8                         // Shift Key active = 1
    #define PS2_FLG_CTRL                   PS2_CTRL     >> 8                         // Ctrl Key active = 1
    #define PS2_FLG_CAPS                   PS2_CAPS     >> 8                         // CAPS active = 1
    #define PS2_FLG_ALT                    PS2_ALT      >> 8                         // ALT flag used as Right CTRL flag, active = 1
    #define PS2_FLG_ALTGR                  PS2_ALT_GR   >> 8                         // ALTGR active = 1
    #define PS2_FLG_GUI                    PS2_GUI      >> 8                         // GUI Key active = 1
    #define PS2_FLG_FUNC                   PS2_FUNCTION >> 8                         // Special Function Keys active = 1
    #define PS2_FLG_BREAK                  PS2_BREAL    >> 8                         // BREAK Key active = 1


    public:

        struct KeyInfo {
                                           uint8_t         keys[MAX_KEYBOARD_DATA_BYTES];
                                           uint8_t         length;
                                           bool            cControl;
                                           esp_hidh_dev_t *hdlDev;
        };

        // Prototypes.
                                           BTHID(void);
        virtual                            ~BTHID(void);
        bool                               setup(t_pairingHandler *handler);
        bool                               openDevice(esp_bd_addr_t bda, esp_hid_transport_t transport, esp_ble_addr_type_t addrType);
        bool                               closeDevice(esp_bd_addr_t bda);
        void                               checkBTDevices(void);
        bool                               setResolution(enum PS2Mouse::PS2_RESOLUTION resolution);
        bool                               setScaling(enum PS2Mouse::PS2_SCALING scaling);
        bool                               setSampleRate(enum PS2Mouse::PS2_SAMPLING rate);
        void                               processBTKeys(void);
        uint16_t                           getKey(uint32_t timeout = 0);

        // Method to register an object method for callback with context.
        template<typename A, typename B>
        void setMouseDataCallback(A func_ptr, B obj_ptr)
        {
            btHIDCtrl.ms.mouseDataCallback = bind(func_ptr, obj_ptr, 1, std::placeholders::_1);
        }

        // Template to aid in conversion of an enum to integer.
        template <typename E> constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
        {
            return static_cast<typename std::underlying_type<E>::type>(e);
        }

    private:
        static constexpr char const * TAG = "BTHID";

        // Structure to hold details of an active or post-active connection.
        typedef struct {
            esp_bd_addr_t                  bda;
            esp_hid_transport_t            transport;
            esp_ble_addr_type_t            addrType;
            esp_hid_usage_t                usage;
            esp_hidh_dev_t                *hidhDevHdl;
            uint32_t                       nextCheckTime;
            bool                           open;
        } t_activeDev;
       
        // Structure to encapsulate a single key map from Bluetooth to PS/2.
        typedef struct {
            uint8_t                        btKeyCode;
            uint16_t                       btCtrl;
            uint8_t                        ps2KeyCode;
            uint16_t                       ps2Ctrl;
        } t_keyMapEntry;        

        // Structure to encapsulate the entire static keyboard mapping table.
        typedef struct {
            t_keyMapEntry                  kme[MAX_BT2PS2_MAP_ENTRIES];
        } t_keyMap;
      
        // Structure to contain a media key map.
        typedef struct {
            uint32_t                       mediaKey;                              // 24bit Media key value.
            uint8_t                        ps2Key;                                // Equivalent PS/2 key for media key.
            uint16_t                       ps2Ctrl;                               // PS/2 translated control flags.
        } t_mediaMapEntry;
       
        // Structure to encapsulate Media key mappings.
        typedef struct {
            t_mediaMapEntry                kme[MAX_BTMEDIA2PS2_MAP_ENTRIES];
        } t_mediaKeyMap;

        // Structure to maintain control variables.
        typedef struct {
            // Array of active devices which connect with the SharpKey.
            std::vector<t_activeDev>       devices;

            // Keyboard handling.
            struct {
                // Queues for storing data in the 2 processing stages.
                xQueueHandle               rawKeyQueue;
                xQueueHandle               keyQueue;

                uint8_t                    lastKeys[MAX_KEYBOARD_DATA_BYTES];     // Required to generate a PS/2 break event when a key is released.
                uint32_t                   lastMediaKey;                          // Required to detect changes in the media control keys, ie. release.
                uint16_t                   btFlags;                               // Bluetooth control flags.
                uint16_t                   ps2Flags;                              // PS/2 translated control flags.
                uint8_t                    statusLED;                             // Keyboard LED state.
                t_keyMapEntry             *kme;                                   // Pointer to the mapping array.
                t_mediaMapEntry           *kmeMedia;                              // Pointer to the media key mapping array.
                int                        kmeRows;                               // Number of entries in the BT to PS/2 mapping table.
                int                        kmeMediaRows;                          // Number of entries in the BT to PS/2 media key mapping table.
            } kbd;

            // Mouse handling.
            struct {
                int                        resolution;                            // PS/2 compatible resolution (pixels per mm) setting.
                int                        scaling;                               // PS/2 compatible scaling (1:1 or 2:1).
                int                        sampleRate;                            // PS/2 compatible sample rate (10 .. 200).
                int                        xDivisor;                              // Divisor on the X plane to scale down the 12bit BT resolution.
                int                        yDivisor;                              // Divisor on the Y plane to scale down the 12bit BT resolution.

                // Callback for streaming processed mouse data to HID handler.
                std::function<void(PS2Mouse::MouseData)> mouseDataCallback;            
            } ms;

            BTHID                          *pThis;
        } t_btHIDCtrl;

        // All control variables are stored in a struct for ease of reference.
        t_btHIDCtrl                        btHIDCtrl;

        // Prototypes.
        static void                        hidh_callback(void * handler_args, esp_event_base_t base, int32_t id, void * event_data);
        void                               pushKeyToFIFO(esp_hid_usage_t src, esp_hidh_dev_t *hdlDev, uint8_t *keys, uint8_t size);
        void                               setStatusLED(esp_hidh_dev_t *dev, uint8_t led);
        void                               clearStatusLED(esp_hidh_dev_t *dev, uint8_t led);
        uint16_t                           mapBTMediaToPS2(uint32_t key);
        uint16_t                           mapBTtoPS2(uint8_t key);
        inline uint32_t milliSeconds(void)
        {
            return( (uint32_t) (clock() ) );
        }

        // Mapping for Media keys. ESP module seperates them but not properly, some media keys are sent as normal key scancodes others as control key bit maps.
        // Hence two mapping tables, one for normal scancodes and one for media codes.
        t_mediaKeyMap   MediaKeyToPS2 = {
        {
            { BT_MEDIA_SEARCH,             PS2_KEY_WEB_SEARCH,              PS2_FLG_NONE,            },
            { BT_MEDIA_HOME,               PS2_KEY_WEB_HOME,                PS2_FLG_NONE,            }, 
            { BT_MEDIA_BRIGHTNESS_UP,      PS2_KEY_WEB_FORWARD,             PS2_FLG_NONE,            }, 
            { BT_MEDIA_BRIGHTNESS_DOWN,    PS2_KEY_WEB_BACK,                PS2_FLG_NONE,            }, 
            { BT_MEDIA_MUTE,               PS2_KEY_MUTE,                    PS2_FLG_NONE,            }, 
            { BT_MEDIA_VOL_DOWN,           PS2_KEY_VOL_DN,                  PS2_FLG_NONE,            }, 
            { BT_MEDIA_VOL_UP,             PS2_KEY_VOL_UP,                  PS2_FLG_NONE,            }, 
            { BT_MEDIA_TRACK_PREV,         PS2_KEY_PREV_TR,                 PS2_FLG_NONE,            }, 
        }};

        // Mapping table between BT Keyboard Scan Codes and PS/2 Keyboard Scan Codes.
        //
        t_keyMap    BTKeyToPS2 = {
        {
            // Bluetooth Key                 Bluetooth Control,            PS/2 Key                      PS/2 Control,                    
            { BT_KEY_A,                      BT_NONE,                      PS2_KEY_A,                    PS2_FLG_NONE,                 },
            { BT_KEY_B,                      BT_NONE,                      PS2_KEY_B,                    PS2_FLG_NONE,                 },
            { BT_KEY_C,                      BT_NONE,                      PS2_KEY_C,                    PS2_FLG_NONE,                 },
            { BT_KEY_D,                      BT_NONE,                      PS2_KEY_D,                    PS2_FLG_NONE,                 },
            { BT_KEY_E,                      BT_NONE,                      PS2_KEY_E,                    PS2_FLG_NONE,                 },
            { BT_KEY_F,                      BT_NONE,                      PS2_KEY_F,                    PS2_FLG_NONE,                 },
            { BT_KEY_G,                      BT_NONE,                      PS2_KEY_G,                    PS2_FLG_NONE,                 },
            { BT_KEY_H,                      BT_NONE,                      PS2_KEY_H,                    PS2_FLG_NONE,                 },
            { BT_KEY_I,                      BT_NONE,                      PS2_KEY_I,                    PS2_FLG_NONE,                 },
            { BT_KEY_J,                      BT_NONE,                      PS2_KEY_J,                    PS2_FLG_NONE,                 },
            { BT_KEY_K,                      BT_NONE,                      PS2_KEY_K,                    PS2_FLG_NONE,                 },
            { BT_KEY_L,                      BT_NONE,                      PS2_KEY_L,                    PS2_FLG_NONE,                 },
            { BT_KEY_M,                      BT_NONE,                      PS2_KEY_M,                    PS2_FLG_NONE,                 },
            { BT_KEY_N,                      BT_NONE,                      PS2_KEY_N,                    PS2_FLG_NONE,                 },
            { BT_KEY_O,                      BT_NONE,                      PS2_KEY_O,                    PS2_FLG_NONE,                 },
            { BT_KEY_P,                      BT_NONE,                      PS2_KEY_P,                    PS2_FLG_NONE,                 },
            { BT_KEY_Q,                      BT_NONE,                      PS2_KEY_Q,                    PS2_FLG_NONE,                 },
            { BT_KEY_R,                      BT_NONE,                      PS2_KEY_R,                    PS2_FLG_NONE,                 },
            { BT_KEY_S,                      BT_NONE,                      PS2_KEY_S,                    PS2_FLG_NONE,                 },
            { BT_KEY_T,                      BT_NONE,                      PS2_KEY_T,                    PS2_FLG_NONE,                 },
            { BT_KEY_U,                      BT_NONE,                      PS2_KEY_U,                    PS2_FLG_NONE,                 },
            { BT_KEY_V,                      BT_NONE,                      PS2_KEY_V,                    PS2_FLG_NONE,                 },
            { BT_KEY_W,                      BT_NONE,                      PS2_KEY_W,                    PS2_FLG_NONE,                 },
            { BT_KEY_X,                      BT_NONE,                      PS2_KEY_X,                    PS2_FLG_NONE,                 },
            { BT_KEY_Y,                      BT_NONE,                      PS2_KEY_Y,                    PS2_FLG_NONE,                 },
            { BT_KEY_Z,                      BT_NONE,                      PS2_KEY_Z,                    PS2_FLG_NONE,                 },
            { BT_KEY_1,                      BT_NONE,                      PS2_KEY_1,                    PS2_FLG_NONE,                 },
            { BT_KEY_2,                      BT_NONE,                      PS2_KEY_2,                    PS2_FLG_NONE,                 },
            { BT_KEY_3,                      BT_NONE,                      PS2_KEY_3,                    PS2_FLG_NONE,                 },
            { BT_KEY_4,                      BT_NONE,                      PS2_KEY_4,                    PS2_FLG_NONE,                 },
            { BT_KEY_5,                      BT_NONE,                      PS2_KEY_5,                    PS2_FLG_NONE,                 },
            { BT_KEY_6,                      BT_NONE,                      PS2_KEY_6,                    PS2_FLG_NONE,                 },
            { BT_KEY_7,                      BT_NONE,                      PS2_KEY_7,                    PS2_FLG_NONE,                 },
            { BT_KEY_8,                      BT_NONE,                      PS2_KEY_8,                    PS2_FLG_NONE,                 },
            { BT_KEY_9,                      BT_NONE,                      PS2_KEY_9,                    PS2_FLG_NONE,                 },
            { BT_KEY_0,                      BT_NONE,                      PS2_KEY_0,                    PS2_FLG_NONE,                 },
            { BT_KEY_ENTER,                  BT_NONE,                      PS2_KEY_ENTER,                PS2_FLG_NONE,                 },
            { BT_KEY_ESC,                    BT_NONE,                      PS2_KEY_ESC,                  PS2_FLG_NONE,                 },
            { BT_KEY_BACKSPACE,              BT_NONE,                      PS2_KEY_BS,                   PS2_FLG_NONE,                 },
            { BT_KEY_TAB,                    BT_NONE,                      PS2_KEY_TAB,                  PS2_FLG_NONE,                 },
            { BT_KEY_SPACE,                  BT_NONE,                      PS2_KEY_SPACE,                PS2_FLG_NONE,                 },
            { BT_KEY_MINUS,                  BT_NONE,                      PS2_KEY_MINUS,                PS2_FLG_NONE,                 },
            { BT_KEY_EQUAL,                  BT_NONE,                      PS2_KEY_EQUAL,                PS2_FLG_NONE,                 },
            { BT_KEY_LEFTBRACE,              BT_NONE,                      PS2_KEY_OPEN_SQ,              PS2_FLG_NONE,                 },
            { BT_KEY_RIGHTBRACE,             BT_NONE,                      PS2_KEY_CLOSE_SQ,             PS2_FLG_NONE,                 },
            { BT_KEY_BACKSLASH,              BT_NONE,                      PS2_KEY_BACK,                 PS2_FLG_NONE,                 },
            { BT_KEY_HASHTILDE,              BT_NONE,                      PS2_KEY_HASH,                 PS2_FLG_NONE,                 },
            { BT_KEY_SEMICOLON,              BT_NONE,                      PS2_KEY_SEMI,                 PS2_FLG_NONE,                 },
            { BT_KEY_APOSTROPHE,             BT_NONE,                      PS2_KEY_APOS,                 PS2_FLG_NONE,                 },
            { BT_KEY_GRAVE,                  BT_NONE,                      PS2_KEY_BTICK,                PS2_FLG_NONE,                 },
            { BT_KEY_COMMA,                  BT_NONE,                      PS2_KEY_COMMA,                PS2_FLG_NONE,                 },
            { BT_KEY_DOT,                    BT_NONE,                      PS2_KEY_DOT,                  PS2_FLG_NONE,                 },
            { BT_KEY_SLASH,                  BT_NONE,                      PS2_KEY_DIV,                  PS2_FLG_NONE,                 },
            { BT_KEY_CAPSLOCK,               BT_NONE,                      PS2_KEY_CAPS,                 PS2_FLG_NONE,                 },
            { BT_KEY_F1,                     BT_NONE,                      PS2_KEY_F1,                   PS2_FLG_NONE,                 },
            { BT_KEY_F2,                     BT_NONE,                      PS2_KEY_F2,                   PS2_FLG_NONE,                 },
            { BT_KEY_F3,                     BT_NONE,                      PS2_KEY_F3,                   PS2_FLG_NONE,                 },
            { BT_KEY_F4,                     BT_NONE,                      PS2_KEY_F4,                   PS2_FLG_NONE,                 },
            { BT_KEY_F5,                     BT_NONE,                      PS2_KEY_F5,                   PS2_FLG_NONE,                 },
            { BT_KEY_F6,                     BT_NONE,                      PS2_KEY_F6,                   PS2_FLG_NONE,                 },
            { BT_KEY_F7,                     BT_NONE,                      PS2_KEY_F7,                   PS2_FLG_NONE,                 },
            { BT_KEY_F8,                     BT_NONE,                      PS2_KEY_F8,                   PS2_FLG_NONE,                 },
            { BT_KEY_F9,                     BT_NONE,                      PS2_KEY_F9,                   PS2_FLG_NONE,                 },
            { BT_KEY_F10,                    BT_NONE,                      PS2_KEY_F10,                  PS2_FLG_NONE,                 },
            { BT_KEY_F11,                    BT_NONE,                      PS2_KEY_F11,                  PS2_FLG_NONE,                 },
            { BT_KEY_F12,                    BT_NONE,                      PS2_KEY_F12,                  PS2_FLG_NONE,                 },
            { BT_KEY_SYSRQ,                  BT_NONE,                      PS2_KEY_PRTSCR,               PS2_FLG_NONE,                 },
            { BT_KEY_SCROLLLOCK,             BT_NONE,                      PS2_KEY_SCROLL,               PS2_FLG_NONE,                 },
            { BT_KEY_PAUSE,                  BT_NONE,                      PS2_KEY_PAUSE,                PS2_FLG_NONE,                 },
            { BT_KEY_INSERT,                 BT_NONE,                      PS2_KEY_INSERT,               PS2_FLG_NONE,                 },
            { BT_KEY_HOME,                   BT_NONE,                      PS2_KEY_HOME,                 PS2_FLG_NONE,                 },
            { BT_KEY_PAGEUP,                 BT_NONE,                      PS2_KEY_PGUP,                 PS2_FLG_NONE,                 },
            { BT_KEY_DELETE,                 BT_NONE,                      PS2_KEY_DELETE,               PS2_FLG_NONE,                 },
            { BT_KEY_END,                    BT_NONE,                      PS2_KEY_END,                  PS2_FLG_NONE,                 },
            { BT_KEY_PAGEDOWN,               BT_NONE,                      PS2_KEY_PGDN,                 PS2_FLG_NONE,                 },
            { BT_KEY_RIGHT,                  BT_NONE,                      PS2_KEY_R_ARROW,              PS2_FLG_NONE,                 },
            { BT_KEY_LEFT,                   BT_NONE,                      PS2_KEY_L_ARROW,              PS2_FLG_NONE,                 },
            { BT_KEY_DOWN,                   BT_NONE,                      PS2_KEY_DN_ARROW,             PS2_FLG_NONE,                 },
            { BT_KEY_UP,                     BT_NONE,                      PS2_KEY_UP_ARROW,             PS2_FLG_NONE,                 },
            { BT_KEY_NUMLOCK,                BT_NONE,                      PS2_KEY_NUM,                  PS2_FLG_NONE,                 },
            { BT_KEY_KPSLASH,                BT_NONE,                      PS2_KEY_KP_DIV,               PS2_FLG_NONE,                 },
            { BT_KEY_KPASTERISK,             BT_NONE,                      PS2_KEY_KP_TIMES,             PS2_FLG_NONE,                 },
            { BT_KEY_KPMINUS,                BT_NONE,                      PS2_KEY_KP_MINUS,             PS2_FLG_NONE,                 },
            { BT_KEY_KPPLUS,                 BT_NONE,                      PS2_KEY_KP_PLUS,              PS2_FLG_NONE,                 },
            { BT_KEY_KPENTER,                BT_NONE,                      PS2_KEY_KP_ENTER,             PS2_FLG_NONE,                 },
            { BT_KEY_KP1,                    BT_NUM_LOCK,                  PS2_KEY_KP1,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP2,                    BT_NUM_LOCK,                  PS2_KEY_KP2,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP3,                    BT_NUM_LOCK,                  PS2_KEY_KP3,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP4,                    BT_NUM_LOCK,                  PS2_KEY_KP4,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP5,                    BT_NUM_LOCK,                  PS2_KEY_KP5,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP6,                    BT_NUM_LOCK,                  PS2_KEY_KP6,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP7,                    BT_NUM_LOCK,                  PS2_KEY_KP7,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP8,                    BT_NUM_LOCK,                  PS2_KEY_KP8,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP9,                    BT_NUM_LOCK,                  PS2_KEY_KP9,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP0,                    BT_NUM_LOCK,                  PS2_KEY_KP0,                  PS2_FLG_NONE,                 },
            { BT_KEY_KPDOT,                  BT_NUM_LOCK,                  PS2_KEY_KP_DOT,               PS2_FLG_NONE,                 },
            { BT_KEY_KP1,                    BT_NONE,                      PS2_KEY_END,                  PS2_FLG_NONE,                 },
            { BT_KEY_KP2,                    BT_NONE,                      PS2_KEY_DN_ARROW,             PS2_FLG_NONE,                 },
            { BT_KEY_KP3,                    BT_NONE,                      PS2_KEY_PGDN,                 PS2_FLG_NONE,                 },
            { BT_KEY_KP4,                    BT_NONE,                      PS2_KEY_L_ARROW,              PS2_FLG_NONE,                 },
            { BT_KEY_KP5,                    BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_KP6,                    BT_NONE,                      PS2_KEY_R_ARROW,              PS2_FLG_NONE,                 },
            { BT_KEY_KP7,                    BT_NONE,                      PS2_KEY_HOME,                 PS2_FLG_NONE,                 },
            { BT_KEY_KP8,                    BT_NONE,                      PS2_KEY_UP_ARROW,             PS2_FLG_NONE,                 },
            { BT_KEY_KP9,                    BT_NONE,                      PS2_KEY_PGUP,                 PS2_FLG_NONE,                 },
            { BT_KEY_KP0,                    BT_NONE,                      PS2_KEY_INSERT,               PS2_FLG_NONE,                 },
            { BT_KEY_KPDOT,                  BT_NONE,                      PS2_KEY_DELETE,               PS2_FLG_NONE,                 },
            { BT_KEY_102ND,                  BT_NONE,                      PS2_KEY_BACK,                 PS2_FLG_NONE,                 },
            { BT_KEY_COMPOSE,                BT_NONE,                      PS2_KEY_MENU,                 PS2_FLG_NONE,                 },
            { BT_KEY_POWER,                  BT_NONE,                      PS2_KEY_POWER,                PS2_FLG_NONE,                 },
            { BT_KEY_KPEQUAL,                BT_NONE,                      PS2_KEY_KP_EQUAL,             PS2_FLG_NONE,                 },
            { BT_KEY_F13,                    BT_NONE,                      PS2_KEY_F13,                  PS2_FLG_NONE,                 },
            { BT_KEY_F14,                    BT_NONE,                      PS2_KEY_F14,                  PS2_FLG_NONE,                 },
            { BT_KEY_F15,                    BT_NONE,                      PS2_KEY_F15,                  PS2_FLG_NONE,                 },
            { BT_KEY_F16,                    BT_NONE,                      PS2_KEY_F16,                  PS2_FLG_NONE,                 },
            { BT_KEY_F17,                    BT_NONE,                      PS2_KEY_F17,                  PS2_FLG_NONE,                 },
            { BT_KEY_F18,                    BT_NONE,                      PS2_KEY_F18,                  PS2_FLG_NONE,                 },
            { BT_KEY_F19,                    BT_NONE,                      PS2_KEY_F19,                  PS2_FLG_NONE,                 },
            { BT_KEY_F20,                    BT_NONE,                      PS2_KEY_F20,                  PS2_FLG_NONE,                 },
            { BT_KEY_F21,                    BT_NONE,                      PS2_KEY_F21,                  PS2_FLG_NONE,                 },
            { BT_KEY_F22,                    BT_NONE,                      PS2_KEY_F22,                  PS2_FLG_NONE,                 },
            { BT_KEY_F23,                    BT_NONE,                      PS2_KEY_F23,                  PS2_FLG_NONE,                 },
            { BT_KEY_F24,                    BT_NONE,                      PS2_KEY_F24,                  PS2_FLG_NONE,                 },
            { BT_KEY_OPEN,                   BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_HELP,                   BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_PROPS,                  BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_FRONT,                  BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_STOP,                   BT_NONE,                      PS2_KEY_STOP,                 PS2_FLG_NONE,                 },
            { BT_KEY_AGAIN,                  BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_UNDO,                   BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_CUT,                    BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_COPY,                   BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_PASTE,                  BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_FIND,                   BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MUTE,                   BT_NONE,                      PS2_KEY_MUTE,                 PS2_FLG_NONE,                 },
            { BT_KEY_VOLUMEUP,               BT_NONE,                      PS2_KEY_VOL_UP,               PS2_FLG_NONE,                 },
            { BT_KEY_VOLUMEDOWN,             BT_NONE,                      PS2_KEY_VOL_DN,               PS2_FLG_NONE,                 },
            { BT_KEY_KPCOMMA,                BT_NONE,                      PS2_KEY_KP_COMMA,             PS2_FLG_NONE,                 },
            { BT_KEY_RO,                     BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_KATAKANAHIRAGANA,       BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_YEN,                    BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_HENKAN,                 BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MUHENKAN,               BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_KPJPCOMMA,              BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_HANGEUL,                BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_HANJA,                  BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_KATAKANA,               BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_HIRAGANA,               BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_ZENKAKUHANKAKU,         BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_KPLEFTPAREN,            BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_KPRIGHTPAREN,           BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            // Control keys.
            { BT_KEY_LEFTCTRL,               BT_NONE,                      PS2_KEY_L_CTRL,               PS2_FLG_FUNC | PS2_FLG_CTRL,  },
            { BT_KEY_LEFTSHIFT,              BT_NONE,                      PS2_KEY_L_SHIFT,              PS2_FLG_FUNC | PS2_FLG_SHIFT, },
            { BT_KEY_LEFTALT,                BT_NONE,                      PS2_KEY_L_ALT,                PS2_FLG_FUNC | PS2_FLG_ALT,   },
            { BT_KEY_LEFTMETA,               BT_NONE,                      PS2_KEY_L_GUI,                PS2_FLG_FUNC | PS2_FLG_GUI,   },
            { BT_KEY_RIGHTCTRL,              BT_NONE,                      PS2_KEY_R_CTRL,               PS2_FLG_FUNC | PS2_FLG_CTRL,  },
            { BT_KEY_RIGHTSHIFT,             BT_NONE,                      PS2_KEY_R_SHIFT,              PS2_FLG_FUNC | PS2_FLG_SHIFT, },
            { BT_KEY_RIGHTALT,               BT_NONE,                      PS2_KEY_R_ALT,                PS2_FLG_FUNC | PS2_FLG_ALTGR, },
            { BT_KEY_RIGHTMETA,              BT_NONE,                      PS2_KEY_R_GUI,                PS2_FLG_FUNC | PS2_FLG_NONE,  },
            // Media keys
            { BT_KEY_MEDIA_PLAYPAUSE,        BT_NONE,                      PS2_KEY_PLAY,                 PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_STOPCD,           BT_NONE,                      PS2_KEY_STOP,                 PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_PREVIOUSSONG,     BT_NONE,                      PS2_KEY_PREV_TR,              PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_NEXTSONG,         BT_NONE,                      PS2_KEY_NEXT_TR,              PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_EJECTCD,          BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_VOLUMEUP,         BT_NONE,                      PS2_KEY_VOL_UP,               PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_VOLUMEDOWN,       BT_NONE,                      PS2_KEY_VOL_DN,               PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_MUTE,             BT_NONE,                      PS2_KEY_MUTE,                 PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_WWW,              BT_NONE,                      PS2_KEY_WEB_SEARCH,           PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_BACK,             BT_NONE,                      PS2_KEY_WEB_BACK,             PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_FORWARD,          BT_NONE,                      PS2_KEY_WEB_FORWARD,          PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_STOP,             BT_NONE,                      PS2_KEY_WEB_STOP,             PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_FIND,             BT_NONE,                      PS2_KEY_WEB_SEARCH,           PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_SCROLLUP,         BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_SCROLLDOWN,       BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_EDIT,             BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_SLEEP,            BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_COFFEE,           BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_REFRESH,          BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
            { BT_KEY_MEDIA_CALC,             BT_NONE,                      0x00,                         PS2_FLG_NONE,                 },
        }};
};

#endif // BT_KEYBOARD_H_
