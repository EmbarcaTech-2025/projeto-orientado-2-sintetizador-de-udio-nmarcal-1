# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(synth_audio C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(synth_audio
  synth_audio.c
  # Adicione aqui outros arquivos fonte se necessário
)

pico_set_program_name(synth_audio "synth_audio")
pico_set_program_version(synth_audio "0.1")

# Configuração de saída padrão
pico_enable_stdio_uart(synth_audio 0)
pico_enable_stdio_usb(synth_audio 1)

# Inclui diretórios adicionais
target_include_directories(synth_audio PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Bibliotecas necessárias para o projeto
target_link_libraries(synth_audio
    pico_stdlib                   # Biblioteca padrão do Pico SDK
    hardware_adc                  # Para leitura do microfone via ADC
    hardware_pwm                  # Para saída de áudio via PWM
    hardware_i2c                  # Para comunicação com o display OLED
    hardware_dma                  # Opcional: para transferência eficiente de dados
    pico_cyw43_arch_none          # Para suporte ao WiFi (se usando Pico W)
)

# Adiciona arquivos de cabeçalho do display OLED
target_sources(synth_audio PRIVATE
    # ${CMAKE_CURRENT_LIST_DIR}/include/ssd1306.c
    ${CMAKE_CURRENT_LIST_DIR}/include/ssd1306_i2c.c
)

# Configurações adicionais para otimização (opcional)
target_compile_options(synth_audio PRIVATE
    -O2                           # Otimização para melhor desempenho
    -Wall                         # Habilita todos os avisos
)

# Gera arquivos adicionais (UF2, HEX, etc.)
pico_add_extra_outputs(synth_audio)

# Configuração específica para Pico W (se necessário)
#if(PICO_BOARD STREQUAL "pico_w")
#    target_link_libraries(synth_audio
#        pico_cyw43_arch_none      # Arquitetura para o chip WiFi
#    )
#endif()