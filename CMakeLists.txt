# CMakeList.txt: projekt CMake dla elementu projekt1, dołącz źródło i zdefiniuj
# tutaj logikę specyficzną dla projektu.
#
cmake_minimum_required (VERSION 3.8)
 
 
project ("TOP_project")
 
 
set(CMAKE_CXX_STANDARD 17)
 
# Włącz przeładowywanie kodu XAML na gorąco dla kompilatorów MSVC, jeśli jest to obsługiwane.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()
 
 
message("set section")
set(CPLEX "C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio2211\\cplex\\include;C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio2211\\concert\\include")
 
foreach (ELEMENT IN LISTS CPLEX)
    message("include: ${ELEMENT}")
endforeach ()
 
 
include_directories(${CPLEX})
set(SOURCE_FILES "main.cpp")
if (CMAKE_BUILD_TYPE MATCHES Debug)
 
    set(CPLEX_LIB_DIR "C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio2211\\cplex\\lib\\x64_windows_msvc14\\stat_mdd;C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio2211\\concert\\lib\\x64_windows_msvc14\\stat_mdd")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
 
    set(CPLEX_LIB_DIR "C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio2211\\cplex\\lib\\x64_windows_msvc14\\stat_mda;C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio2211\\concert\\lib\\x64_windows_msvc14\\stat_mda")
endif ()
 
message("lib directories section")
foreach (ELEMENT IN LISTS CPLEX_LIB_DIR)
    message("include: ${ELEMENT}")
endforeach ()
set(CPLEX_LIB "cplex2211;ilocplex;concert")
message("libs section")
foreach (ELEMENT IN LISTS CPLEX_LIB)
    message("include: ${ELEMENT}")
endforeach ()
 
add_executable(TOP_project ${SOURCE_FILES})
#set_property(TARGET blendCPLEX PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_link_options(TOP_project PRIVATE "/NODEFAULTLIB:MSVCRT")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    target_link_options(TOP_project PRIVATE "/NODEFAULTLIB:MSVCRTD")
endif()
 
target_link_directories(TOP_project PRIVATE ${CPLEX_LIB_DIR})
target_link_libraries(TOP_project PUBLIC ${CPLEX_LIB})