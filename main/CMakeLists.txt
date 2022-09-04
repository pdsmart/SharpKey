set(COMPONENT_SRCS SharpKey.cpp NVS.cpp LED.cpp SWITCH.cpp KeyInterface.cpp MZ2528.cpp X1.cpp X68K.cpp Mouse.cpp MZ5665.cpp PC9801.cpp HID.cpp WiFi.cpp PS2KeyAdvanced.cpp PS2Mouse.cpp BT.cpp BTHID.cpp esp_efuse_custom_table.c)
set(COMPONENT_ADD_INCLUDEDIRS "." "include")

register_component()
