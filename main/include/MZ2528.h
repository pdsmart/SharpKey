/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Name:            MZ2528.h
// Created:         Mar 2022
// Version:         v1.0
// Author(s):       Philip Smart
// Description:     Header for the MZ-2500/MZ-2800 PS/2 logic.
// Credits:         
// Copyright:       (c) 2019-2022 Philip Smart <philip.smart@net2net.org>
//
// History:         Mar 2022 - Initial write.
//            v1.01 May 2022 - Initial release version.
//            v1.02 Jun 2022 - Updates to reflect bluetooth.
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

#ifndef MZ2528_H
#define MZ2528_H

// Include the specification class.
#include "KeyInterface.h"
#include "NVS.h"
#include "LED.h"
#include "HID.h"
#include <vector>
#include <map>

// NB: Macros definitions put inside class for clarity, they are still global scope.

// Encapsulate the MZ-2500/MZ-2800 interface.
class MZ2528  : public KeyInterface {
    // Macros.
    //
    #define NUMELEM(a)                      (sizeof(a)/sizeof(a[0]))

    // Constants.
    #define MZ2528IF_VERSION                1.02
    #define MZ2528IF_KEYMAP_FILE            "MZ2528_KeyMap.BIN"
    #define PS2TBL_MZ_MAXROWS               165
    #define PS2TBL_MZ_MAX_MKROW             3
    #define PS2TBL_MZ_MAX_BRKROW            2
    
    // PS2 Flag definitions.
    #define PS2CTRL_NONE                    0x00                    // No keys active = 0
    #define PS2CTRL_SHIFT                   0x01                    // Shfit Key active = 1
    #define PS2CTRL_CTRL                    0x02                    // Ctrl Key active = 1
    #define PS2CTRL_CAPS                    0x04                    // CAPS active = 1
    #define PS2CTRL_ALT                     0x08                    // ALT active = 1
    #define PS2CTRL_ALTGR                   0x10                    // ALTGR active = 1
    #define PS2CTRL_GUI                     0x20                    // GUI Key active = 1
    #define PS2CTRL_FUNC                    0x40                    // Special Function Keys active = 1
    #define PS2CTRL_BREAK                   0x80                    // BREAK Key active = 1
    #define PS2CTRL_EXACT                   0x80                    // EXACT Match active = 1
    
    // The MZ-2500 machine can emulate 3 models, the MZ-80B, MZ-2000 and the MZ-2500. The MZ-2800 provides a new mode as well as the MZ-2500 mode and each has slight
    // keyboard differences. This requires tagging of machine specific mappings. Normally a mapping would be MZ_ALL, ie. applied to all models, but if a machine specific
    // mapping appears and it matches the current machine mode, this mapping is chosen.
    #define MZ_ALL                          0xFF
    #define MZ_80B                          0x01
    #define MZ_2000                         0x02
    #define MZ_2500                         0x04
    #define MZ_2800                         0x08
   
    // Keyboard mapping table select list for target machine.
    #define MZ2528_SEL_ALL                  "ALL"
    #define MZ2528_SEL_MZ_80B               "MZ80B"
    #define MZ2528_SEL_MZ_2000              "MZ2000"
    #define MZ2528_SEL_MZ_2500              "MZ2500"
    #define MZ2528_SEL_MZ_2800              "MZ2800"
    
    // The initial mapping is made inside the PS2KeyAdvanced class from Scan Code Set 2 to ASCII
    // for a selected keyboard. Special functions are detected and combined inside this module
    // before mapping with the table below to MZ Scan Matrix.
    // ie. PS/2 Scan Code -> ASCII + Flags -> MZ Scan Matrix


    // Keyboard mapping table column names.
    #define PS2TBL_PS2KEYCODE_NAME          "PS/2 KeyCode"
    #define PS2TBL_PS2CTRL_NAME             "PS/2 Control Key"
    #define PS2TBL_KEYBOARDMODEL_NAME       "For Keyboard"
    #define PS2TBL_MACHINE_NAME             "For Host Model"
    #define PS2TBL_MZ_MK_ROW1_NAME          "Make Row 1"
    #define PS2TBL_MZ_MK_KEY1_NAME          "Key 1"
    #define PS2TBL_MZ_MK_ROW2_NAME          "Row 2"
    #define PS2TBL_MZ_MK_KEY2_NAME          "Key 2"
    #define PS2TBL_MZ_MK_ROW3_NAME          "Row 3"
    #define PS2TBL_MZ_MK_KEY3_NAME          "Key 3"
    #define PS2TBL_MZ_BRK_ROW1_NAME         "Break Row 1"
    #define PS2TBL_MZ_BRK_KEY1_NAME         "Key 1"
    #define PS2TBL_MZ_BRK_ROW2_NAME         "Row 2"
    #define PS2TBL_MZ_BRK_KEY2_NAME         "Key 2"

    // Keyboard mapping table column types.
    #define PS2TBL_PS2KEYCODE_TYPE          "hex"
    #define PS2TBL_PS2CTRL_TYPE             "custom_cbp_ps2ctrl"
    #define PS2TBL_KEYBOARDMODEL_TYPE       "custom_cbp_keybmodel"
    #define PS2TBL_MACHINE_TYPE             "custom_cbp_machine"
    #define PS2TBL_MZ_MK_ROW1_TYPE          "custom_rdp_mzrow"
    #define PS2TBL_MZ_MK_KEY1_TYPE          "hex"
    #define PS2TBL_MZ_MK_ROW2_TYPE          "custom_rdp_mzrow"
    #define PS2TBL_MZ_MK_KEY2_TYPE          "hex"
    #define PS2TBL_MZ_MK_ROW3_TYPE          "custom_rdp_mzrow"
    #define PS2TBL_MZ_MK_KEY3_TYPE          "hex"
    #define PS2TBL_MZ_BRK_ROW1_TYPE         "custom_rdp_mzrow"
    #define PS2TBL_MZ_BRK_KEY1_TYPE         "hex"
    #define PS2TBL_MZ_BRK_ROW2_TYPE         "custom_rdp_mzrow"
    #define PS2TBL_MZ_BRK_KEY2_TYPE         "hex"

     // Keyboard mapping table select list for PS2CTRL.
    #define PS2TBL_PS2CTRL_SEL_NONE         "NONE"
    #define PS2TBL_PS2CTRL_SEL_SHIFT        "SHIFT"
    #define PS2TBL_PS2CTRL_SEL_CTRL         "CTRL"
    #define PS2TBL_PS2CTRL_SEL_CAPS         "CAPS"
    #define PS2TBL_PS2CTRL_SEL_ALT          "ALT"
    #define PS2TBL_PS2CTRL_SEL_ALTGR        "ALTGR"
    #define PS2TBL_PS2CTRL_SEL_GUI          "GUI"
    #define PS2TBL_PS2CTRL_SEL_FUNC         "FUNC"
    #define PS2TBL_PS2CTRL_SEL_EXACT        "EXACT"

    // Keyboard mapping table select list for Model of keyboard.
    #define KEYMAP_SEL_STANDARD             "ALL"
    #define KEYMAP_SEL_UK_WYSE_KB3926       "UK_WYSE_KB3926"
    #define KEYMAP_SEL_JAPAN_OADG109        "JAPAN_OADG109"
    #define KEYMAP_SEL_JAPAN_SANWA_SKBL1    "JAPAN_SANWA_SKBL1"
    #define KEYMAP_SEL_NOT_ASSIGNED_4       "KEYBOARD_4"
    #define KEYMAP_SEL_NOT_ASSIGNED_5       "KEYBOARD_5"
    #define KEYMAP_SEL_NOT_ASSIGNED_6       "KEYBOARD_6"
    #define KEYMAP_SEL_UK_PERIBOARD_810     "UK_PERIBOARD_810"
    #define KEYMAP_SEL_UK_OMOTON_K8508      "UK_OMOTON_K8508"
   
    // Keyboard models. The base on which this interface was created was a Wyse KB3926 PS/2 Keyboard and this is deemed STANDARD. Other models need to insert difference maps
    // prior to the STANDARD entry along with the keyboard model so that it is processed first thus allowing differing keyboards with different maps.
    #define KEYMAP_STANDARD                 0xFF
    #define KEYMAP_UK_WYSE_KB3926           0x01
    #define KEYMAP_JAPAN_OADG109            0x02
    #define KEYMAP_JAPAN_SANWA_SKBL1        0x04
    #define KEYMAP_NOT_ASSIGNED_4           0x08
    #define KEYMAP_NOT_ASSIGNED_5           0x10
    #define KEYMAP_NOT_ASSIGNED_6           0x20
    #define KEYMAP_UK_PERIBOARD_810         0x40
    #define KEYMAP_UK_OMOTON_K8508          0x80

    public:
        // Prototypes.
                                        MZ2528(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID, const char *fsPath);
                                        MZ2528(NVS *hdlNVS, HID *hdlHID, const char *fsPath);
                                        MZ2528(void);
                                        ~MZ2528(void);
        bool                            createKeyMapFile(std::fstream &outFile);
        bool                            storeDataToKeyMapFile(std::fstream &outFile, char *data, int size);
        bool                            storeDataToKeyMapFile(std::fstream & outFile, std::vector<uint32_t>& dataArray);
        bool                            closeAndCommitKeyMapFile(std::fstream &outFile, bool cleanupOnly);
        std::string                     getKeyMapFileName(void) { return(MZ2528IF_KEYMAP_FILE); };
        void                            getKeyMapHeaders(std::vector<std::string>& headerList);
        void                            getKeyMapTypes(std::vector<std::string>& typeList);
        bool                            getKeyMapSelectList(std::vector<std::pair<std::string, int>>& selectList, std::string option);
        bool                            getKeyMapData(std::vector<uint32_t>& dataArray, int *row, bool start);

        // Overloaded method to see if the interface must enter suspend mode, either triggered by an external event or internal.
        //
        inline bool suspendRequested(void)
        {
            return(this->suspend);
        }
       
//        // Method to overload the suspend mechanism and include the core release mechanism. Core release is needed in order to use ESP32 API's such as NVS.
//        // The method is inline to avoid a call overhead as it is generally used in time sensitive interface timing.
//        inline void yield(uint32_t delay)
//        {
//            // If suspended, go into a permanent loop until the suspend flag is reset.
//            if(this->suspend)
//            {
//                this->suspended = true;
//                while(this->suspend)
//                {
//                    vTaskDelay(100);
//                }
//                this->suspended = false;
//            } else
//            // Otherwise just delay by the required amount for timing and to give other threads a time slice.
//            {
 //               vTaskDelay(delay);
//            }
//            return;
//        }

        // Method to return the class version number.
        float version(void)
        {
            return(MZ2528IF_VERSION);
        }

    protected:

    private:
        // Prototypes.
                  void                  updateMirrorMatrix(void);
                  uint32_t              mapKey(uint16_t scanCode);
        IRAM_ATTR static void           mz25Interface(void *pvParameters );
        IRAM_ATTR static void           mz28Interface(void *pvParameters );
        IRAM_ATTR static void           hidInterface(void *pvParameters );
                  void                  selectOption(uint8_t optionCode);
        bool                            loadKeyMap();
        bool                            saveKeyMap(void);
        void                            init(uint32_t ifMode, NVS *hdlNVS, LED *hdlLED, HID *hdlHID);
        void                            init(NVS *hdlNVS, HID *hdlHID);

        // Overload the base yield method to include suspension of the PS/2 Keyboard interface. This interface uses interrupts which are not mutex protected and clash with the
        // WiFi API methods.
//        inline void yield(uint32_t delay)
//        {
//            // If suspended, go into a permanent loop until the suspend flag is reset.
//            if(this->suspend)
//            {
//                // Suspend the keyboard interface.
//                Keyboard->suspend(true);
//
//                // Use the base method logic.
//                KeyInterface::yield(delay);
//                
//                // Release the keyboard interface.
//                Keyboard->suspend(false);
//            } else
//            // Otherwise just delay by the required amount for timing and to give other threads a time slice.
//            {
//                KeyInterface::yield(delay);
//            }
//            return;
//        }

        // Structure to encapsulate a single key map from PS/2 to MZ-2500/MZ-2800.
        typedef struct {
            uint8_t                     ps2KeyCode;
            uint8_t                     ps2Ctrl;
            uint8_t                     keyboardModel;
            uint8_t                     machine;
            uint8_t                     mkRow[PS2TBL_MZ_MAX_MKROW];
            uint8_t                     mkKey[PS2TBL_MZ_MAX_MKROW];
            uint8_t                     brkRow[PS2TBL_MZ_MAX_BRKROW];
            uint8_t                     brkKey[PS2TBL_MZ_MAX_BRKROW];
        } t_keyMapEntry;

        // Structure to encapsulate the entire static keyboard mapping table.
        typedef struct {
            t_keyMapEntry               kme[PS2TBL_MZ_MAXROWS];
        } t_keyMap;

        // Structure to maintain the MZ2528 interface configuration data. This data is persisted through powercycles as needed.
        typedef struct {
            struct {
                uint8_t                 activeKeyboardMap;      // Model of keyboard a keymap entry is applicable to.
                uint8_t                 activeMachineModel;     // Machine model a keymap entry is applicable to.
            } params;
        } t_mzConfig;
       
        // Configuration data.
        t_mzConfig                      mzConfig;

        // Structure to manage the translated key matrix. This is updated by the ps2Interface thread and read by the mzInterface thead.
        typedef struct {
            uint8_t                     strobeAll;              // Strobe All flag, 16 possible rows have the same column AND'd together to create this 8bit map. It is used to see if any key has been pressed.
            uint32_t                    strobeAllAsGPIO;        // Strobe All signal but as a GPIO bit map to save time in the interface thread.
            uint8_t                     keyMatrix[16];          // Key matrix as a 16x8 matrix.
            uint32_t                    keyMatrixAsGPIO[16];    // Key matrix mapped as GPIO bits to save time in the interface thread.
            bool                        mode2500;
            bool                        optionSelect;           // Flag to indicate a user requested keyboard configuration option is being selected.
            std::string                 fsPath;                 // Path on the underlying filesystem where storage is mounted and accessible.
            t_keyMapEntry              *kme;                    // Pointer to an array in memory to contain PS2 to MZ-2500/MZ-2800 mapping values.
            int                         kmeRows;                // Number of rows in the kme table.
            std::string                 keyMapFileName;         // Name of file where extension or replacement key map entries are stored.
            bool                        noKeyPressed;           // Flag to indicate no key has been pressed.
            bool                        persistConfig;          // Flag to request saving of the config into NVS storage.
        } t_mzControl;

        // Thread handles - one per function, ie. HID interface and host target interface.
        TaskHandle_t                    TaskHostIF = NULL;
        TaskHandle_t                    TaskHIDIF  = NULL;

        // Control structure to control interaction and mapping of keys for the host.
        t_mzControl                     mzControl;
       
        // Spin lock mutex to hold a coresied to an uninterruptable method. This only works on dual core ESP32's.
        portMUX_TYPE                    mzMutex;

        // Flag to indicate host interface should yield the CPU.
        volatile bool                   yieldHostInterface;

//        // Keyboard object for PS/2 data retrieval and management.
//        PS2KeyAdvanced                  *Keyboard;

        // HID object, used for keyboard input.
//        HID                             *hid;

        // Lookup table to matrix row/column co-ordinates.
        //
        // Given that the MZ-2500 can emulate 3 machines and each machine has it's own mapping, differences are tagged by machine name, ie. ALL, MZ80B, MZ2000, MZ2500
        //
        // If a PS2 key is matched, then the Matrix is updated using MK_ROW to point into the array with MK_KEY being the column value, equivalent of strobe line and 
        // the required KEY bits to be set. Upto 3 matrix bits can be set (3 key presses on the MZ-2500 keyboard) per PS/2 key. Upto 2 matrix releases can be set per 
        // PS/2 key. A key release is used when a modifier may already have been pressed, ie. SHIFT and it needs to be released to set the required key into the matrix.
        // A set bit = 1, reset bits = 0 but is inverted in the actual matrix (1 = inactive, 0 = active), this applies for releases two, if bit = 1 then that key will be released.
        // The table is scanned for a match from top to bottom. The first match is used so order is important. Japanese characters are being added as I gleam more information.
        
        ///////////////////////////
        // MZ-2500 Keyboard Map. //
        ///////////////////////////
        //
        // Row     D7        D6        D5        D4        D3        D2        D1        D0
        //----------------------------------------------------------------------------------
        //  0      F8        F7        F6        F5        F4        F3        F2        F1
        //  1      KP -      KP +      KP .      KP ,      KP 9      KP 8      F1O       F9
        //  2      KP 7      KP 6      KP 5      KP 4      KP 3      KP 2      KP 1      KP 0
        //  3      BREAK     RIGHT     LEFT      DOWN      UP        RETURN    SPACE     TAB
        //  4       G         F         E        D         C         B         A         / ?
        //  5       O         N         M        L         K         J         I         H
        //  6       W         V         U        T         S         R         Q         P
        //  7       , <       . >       _        YEN |     ^ '¿      Z ¿       Y         X ¿ 
        //  8       7 '       6 &       5 %      4 $       3 #       2 "       1 !       0
        //  9                 [ {       @ `      - =       ; +       : *       9 )       8 (
        // 10       KP /      KP *      ESC      BACKSPACE INST/DEL  CLR/HOME  COPY      ] }
        // 11                                    CTRL      KANA      SHIFT     LOCK      GRAPH
        // 12                                                                  KJ2       KJ1 
        // 13                                                                  HELP      ARGO 
        //
        // Col      0        1         2         3         4         5         6         7        8        9        10       11       12      13
        // --------------------------------------------------------------------------------------------------------------------------------------
        // D0       F1       F9        KP 0      TAB       / ?       H         P         X        0        8 (      ] }      GRAPH    KJ1     ARGO
        // D1       F2       F10       KP 1      SPACE     A         I         Q         Y        1 !      9 )      COPY     LOCK     KJ2     HELP
        // D2       F3       KP 8      KP 2      RETURN    B         J         R         Z        2 "      : *      CLR/HOME SHIFT
        // D3       F4       KP 9      KP 3      UP        C         K         S         ^ '¿     3 #      ; +      INST/DEL KANA
        // D4       F5       KP ,      KP 4      DOWN      D         L         T         YEN |    4 $      - =      BACKSPACE CTRL
        // D5       F6       KP .      KP 5      LEFT      E         M         U         _        5 %      @ `      ESC
        // D6       F7       KP +      KP 6      RIGHT     F         N         V         . >      6 &      [ {      KP *
        // D7       F8       KP -      KP 7      BREAK     G         O         W         , <      7 '               KP /
        //
        // This initial mapping is for the UK Wyse KB-3926 PS/2 keyboard and his equates to KEYMAP_STANDARD.
        //
        t_keyMap                        PS2toMZ = {
        {
          //                                                                                                                              < Keys to be applied on match                    >       < Keys to be reset on match      >
          //  PS2 Code         PS2 Ctrl (Flags to Match)                       Keyboard Model                    Machine                  MK_ROW1  MK_ROW2  MK_ROW3  MK_KEY1  MK_KEY2  MK_KEY3     BRK_ROW1 BRK_ROW2 BRK_KEY1 BRK_KEY2
            { PS2_KEY_F1,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x00,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F1
            { PS2_KEY_F2,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x00,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F2
            { PS2_KEY_F3,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x00,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F3
            { PS2_KEY_F4,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x00,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F4
            { PS2_KEY_F5,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x00,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F5
            { PS2_KEY_F6,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x00,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F6
            { PS2_KEY_F7,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x00,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F7
            { PS2_KEY_F8,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x00,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F8
            { PS2_KEY_F9,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x01,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F9
            { PS2_KEY_F10,       PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x01,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F10
            { PS2_KEY_F11,       PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0D,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // HELP
            { PS2_KEY_F12,       PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // COPY
            { PS2_KEY_TAB,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // TAB
        	{ PS2_KEY_0,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0x0B,    0xFF,    0x02,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Close Right Bracket )
        	{ PS2_KEY_0,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 0
        	{ PS2_KEY_1,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0x0B,    0xFF,    0x02,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Exclamation
        	{ PS2_KEY_1,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 1
        	{ PS2_KEY_2,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0x0B,    0xFF,    0x04,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Double quote.
        	{ PS2_KEY_2,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 2
            { PS2_KEY_3,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0x0B,    0xFF,    0x08,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Pound Sign -> Hash
        	{ PS2_KEY_3,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 3
        	{ PS2_KEY_4,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0x0B,    0xFF,    0x10,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Dollar
        	{ PS2_KEY_4,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 4
        	{ PS2_KEY_5,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0x0B,    0xFF,    0x20,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Percent
        	{ PS2_KEY_5,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 5
        	{ PS2_KEY_6,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0x0B,    0xFF,    0x04,    0xFF,     }, // Kappa
        	{ PS2_KEY_6,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 6
        	{ PS2_KEY_7,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0x0B,    0xFF,    0x40,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Ampersand
        	{ PS2_KEY_7,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 7
        	{ PS2_KEY_8,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0x0B,    0xFF,    0x04,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Star
        	{ PS2_KEY_8,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 8
        	{ PS2_KEY_9,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0x0B,    0xFF,    0x01,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Open Left Bracket (
        	{ PS2_KEY_9,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 9
        	{ PS2_KEY_A,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // a
        	{ PS2_KEY_A,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // A
            { PS2_KEY_B,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // b
            { PS2_KEY_B,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // B
            { PS2_KEY_C,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // c
            { PS2_KEY_C,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // C
            { PS2_KEY_D,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // d
            { PS2_KEY_D,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // D
            { PS2_KEY_E,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // e
            { PS2_KEY_E,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // E
            { PS2_KEY_F,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // f
            { PS2_KEY_F,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // F
            { PS2_KEY_G,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // g
            { PS2_KEY_G,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // G
            { PS2_KEY_H,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // h
            { PS2_KEY_H,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // H
            { PS2_KEY_I,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // i
            { PS2_KEY_I,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // I
            { PS2_KEY_J,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // j
            { PS2_KEY_J,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // J
            { PS2_KEY_K,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // k
            { PS2_KEY_K,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // K
            { PS2_KEY_L,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // l
            { PS2_KEY_L,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // L
            { PS2_KEY_M,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // m
            { PS2_KEY_M,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // M
            { PS2_KEY_N,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // n
            { PS2_KEY_N,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // N
            { PS2_KEY_O,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // o
            { PS2_KEY_O,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x05,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // O
            { PS2_KEY_P,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // p
            { PS2_KEY_P,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // P
            { PS2_KEY_Q,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // q
            { PS2_KEY_Q,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Q
            { PS2_KEY_R,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // r
            { PS2_KEY_R,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // R
            { PS2_KEY_S,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // s
            { PS2_KEY_S,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // S
            { PS2_KEY_T,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // t
            { PS2_KEY_T,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // T
            { PS2_KEY_U,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // u
            { PS2_KEY_U,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // U
            { PS2_KEY_V,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // v
            { PS2_KEY_V,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // V
            { PS2_KEY_W,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // w
            { PS2_KEY_W,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x06,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // W
            { PS2_KEY_X,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // x
            { PS2_KEY_X,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // X
            { PS2_KEY_Y,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // y
            { PS2_KEY_Y,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Y
            { PS2_KEY_Z,         PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // z
            { PS2_KEY_Z,         PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Z
          //  PS2 Code         PS2 Ctrl (Flags to Match)                       Keyboard Model                    Machine                  MK_ROW1  MK_ROW2  MK_ROW3  MK_KEY1  MK_KEY2  MK_KEY3     BRK_ROW1 BRK_ROW2 BRK_KEY1 BRK_KEY2
            { PS2_KEY_SPACE,     PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Space
            { PS2_KEY_COMMA,     PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0x0B,    0xFF,    0x80,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Less Than <
            { PS2_KEY_COMMA,     PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Comma ,
            { PS2_KEY_SEMI,      PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0x0B,    0xFF,    0x04,    0xFF,     }, // Colon :
            { PS2_KEY_SEMI,      PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Semi-Colon ;
            { PS2_KEY_DOT,       PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0x0B,    0xFF,    0x40,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Greater Than >
            { PS2_KEY_DOT,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Full stop .
            { PS2_KEY_DIV,       PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_2000,                 0x07,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0x0B,    0xFF,    0x04,    0xFF,     }, // Question ?
            { PS2_KEY_DIV,       PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_80B,                  0x07,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0x0B,    0xFF,    0x04,    0xFF,     }, // Question ?
            { PS2_KEY_DIV,       PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0x0B,    0xFF,    0x01,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Question ?
            { PS2_KEY_DIV,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x04,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Divide /
            { PS2_KEY_MINUS,     PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_2000,                 0x08,    0x0B,    0xFF,    0x01,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Upper bar
            { PS2_KEY_MINUS,     PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_80B,                  0x08,    0x0B,    0xFF,    0x01,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Upper bar
            { PS2_KEY_MINUS,     PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0x0B,    0xFF,    0x04,    0xFF,     }, // Underscore
            { PS2_KEY_MINUS,     PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, //

            { PS2_KEY_APOS,      PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_80B,                  0x09,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0x0B,    0xFF,    0x04,    0xFF,     }, // At @
            { PS2_KEY_APOS,      PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0x0B,    0xFF,    0x04,    0xFF,     }, // At @
            { PS2_KEY_APOS,      PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0x0B,    0xFF,    0x80,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Single quote '

            { PS2_KEY_OPEN_SQ,   PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0x0B,    0xFF,    0x40,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Open Left Brace {
            { PS2_KEY_OPEN_SQ,   PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Open Left Square Bracket [
            { PS2_KEY_EQUAL,     PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0x0B,    0xFF,    0x08,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Plus +
            { PS2_KEY_EQUAL,     PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0x0B,    0xFF,    0x10,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Equal =
            { PS2_KEY_CAPS,      PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0B,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // LOCK
            { PS2_KEY_ENTER,     PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // ENTER/RETURN
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0x0B,    0xFF,    0x01,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Close Right Brace }
            { PS2_KEY_CLOSE_SQ,  PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Close Right Square Bracket ]
            { PS2_KEY_BACK,      PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0x0B,    0xFF,    0x10,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // 
            { PS2_KEY_BACK,      PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Back slash maps to Yen
            { PS2_KEY_BTICK,     PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0x07,    0x0B,    0xFF,    0x10,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Pipe
            { PS2_KEY_BTICK,     PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x09,    0x0B,    0xFF,    0x20,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Back tick `
            { PS2_KEY_HASH,      PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_2000,                 0x07,    0x0B,    0xFF,    0x08,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Tilde
            { PS2_KEY_HASH,      PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_80B,                  0x07,    0x0B,    0xFF,    0x08,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Tilde
            { PS2_KEY_HASH,      PS2CTRL_SHIFT,                                KEYMAP_STANDARD,                  MZ_ALL,                  0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Tilde has no mapping.
            { PS2_KEY_HASH,      PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x08,    0x0B,    0xFF,    0x08,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Hash
            { PS2_KEY_BS,        PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Backspace
            { PS2_KEY_ESC,       PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // ESCape
            { PS2_KEY_SCROLL,    PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Not assigned.
            { PS2_KEY_INSERT,    PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0x0B,    0xFF,    0x08,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // INSERT
            { PS2_KEY_HOME,      PS2CTRL_FUNC | PS2CTRL_SHIFT | PS2CTRL_EXACT, KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0x0B,    0xFF,    0x04,    0x04,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // CLR
            { PS2_KEY_HOME,      PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // HOME
            { PS2_KEY_PGUP,      PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Not assigned.
            { PS2_KEY_DELETE,    PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // DELETE
            { PS2_KEY_END,       PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Not assigned.
            { PS2_KEY_PGDN,      PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_80B|MZ_2000|MZ_2500,  0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Not mapped
            { PS2_KEY_PGDN,      PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_2800,                 0x0C,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Japanese Key - Previous
            { PS2_KEY_UP_ARROW,  PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Up Arrow
            { PS2_KEY_L_ARROW,   PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Left Arrow
            { PS2_KEY_DN_ARROW,  PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Down Arrow
            { PS2_KEY_R_ARROW,   PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Right Arrow
            { PS2_KEY_NUM,       PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Not assigned.
                                                                                                                                                                                                                     
            // Keypad.                                                                                                                                                                                               
            { PS2_KEY_KP0,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x02,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 0
            { PS2_KEY_KP1,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x02,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 1
            { PS2_KEY_KP2,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x02,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 2
            { PS2_KEY_KP3,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x02,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 3
            { PS2_KEY_KP4,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x02,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 4
            { PS2_KEY_KP5,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x02,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 5
            { PS2_KEY_KP6,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x02,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 6
            { PS2_KEY_KP7,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x02,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 7
            { PS2_KEY_KP8,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x01,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 8
            { PS2_KEY_KP9,       PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x01,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad 9
            { PS2_KEY_KP_COMMA,  PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x01,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad Comma , 
            { PS2_KEY_KP_DOT,    PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x01,    0xFF,    0xFF,    0x20,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad Full stop . 
            { PS2_KEY_KP_PLUS,   PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x01,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad Plus + 
            { PS2_KEY_KP_MINUS,  PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x01,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad Minus - 
            { PS2_KEY_KP_TIMES,  PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0xFF,    0xFF,    0x40,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad Times * 
            { PS2_KEY_KP_DIV,    PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0A,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad Divide /
            { PS2_KEY_KP_ENTER,  PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Keypad Ebter /
                                                                                                                                                                                                                     
          //  PS2 Code         PS2 Ctrl (Flags to Match)                       Keyboard Model                    Machine                  MK_ROW1  MK_ROW2  MK_ROW3  MK_KEY1  MK_KEY2  MK_KEY3     BRK_ROW1 BRK_ROW2 BRK_KEY1 BRK_KEY2
            // Special keys.                                                                                                                                                                                         
            { PS2_KEY_PRTSCR,    PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0D,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // ARGO KEY
            { PS2_KEY_PAUSE,     PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x03,    0xFF,    0xFF,    0x80,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // BREAK KEY
            { PS2_KEY_L_GUI,     PS2CTRL_FUNC | PS2CTRL_GUI,                   KEYMAP_STANDARD,                  MZ_ALL,                  0x0B,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // GRAPH KEY
            { PS2_KEY_L_ALT,     PS2CTRL_FUNC | PS2CTRL_ALT,                   KEYMAP_STANDARD,                  MZ_ALL,                  0x0C,    0xFF,    0xFF,    0x01,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // KJ1 Sentence
            { PS2_KEY_R_ALT,     PS2CTRL_FUNC | PS2CTRL_ALTGR,                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0C,    0xFF,    0xFF,    0x02,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // KJ2 Transform
            { PS2_KEY_R_GUI,     PS2CTRL_FUNC | PS2CTRL_GUI,                   KEYMAP_STANDARD,                  MZ_ALL,                  0x0B,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // KANA KEY
            { PS2_KEY_MENU,      PS2CTRL_FUNC | PS2CTRL_GUI,                   KEYMAP_STANDARD,                  MZ_ALL,                  0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Not assigned.
            // Modifiers are last, only being selected if an earlier match isnt made.                                                                                                                                
            { PS2_KEY_L_SHIFT,   PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0B,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     },
            { PS2_KEY_R_SHIFT,   PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0B,    0xFF,    0xFF,    0x04,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     },
            { PS2_KEY_L_CTRL,    PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0x0B,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     },
            { PS2_KEY_R_CTRL,    PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_80B|MZ_2000|MZ_2500,  0x0B,    0xFF,    0xFF,    0x10,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Map to Control
            { PS2_KEY_R_CTRL,    PS2CTRL_FUNC,                                 KEYMAP_STANDARD,                  MZ_2800,                 0x0C,    0xFF,    0xFF,    0x08,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     }, // Japanese Key - Cancel
        	{ 0,                 PS2CTRL_NONE,                                 KEYMAP_STANDARD,                  MZ_ALL,                  0xFF,    0xFF,    0xFF,    0xFF,    0xFF,    0xFF,       0xFF,    0xFF,    0xFF,    0xFF,     },
        }};
};

#endif // MZ2528_H
