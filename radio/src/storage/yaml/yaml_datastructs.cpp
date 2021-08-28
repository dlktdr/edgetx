
#include "opentx.h"
#include "yaml_node.h"

#include "yaml_datastructs_funcs.cpp"

#if defined(PCBX10)
 #include "yaml_datastructs_x10.cpp"
#elif defined(PCBX12S)
 #include "yaml_datastructs_x12s.cpp"
#elif defined(PCBX7)
 #include "yaml_datastructs_x7.cpp"
#elif defined(PCBX9LITE)
 #include "yaml_datastructs_x9lite.cpp"
#elif defined(PCBX9LITES)
 #include "yaml_datastructs_x9lites.cpp"
#elif defined(PCBXLITE)
 #include "yaml_datastructs_xlite.cpp"
#elif defined(PCBXLITES)
 #include "yaml_datastructs_xlites.cpp"
#elif defined(PCBX9E)
 #include "yaml_datastructs_x9e.cpp"
#elif defined(PCBX9D)
 #include "yaml_datastructs_x9d.cpp"
#elif defined(PCBX9DP)
 #include "yaml_datastructs_x9dp.cpp"
#else
#error "Board not supported by YAML storage"
#endif
