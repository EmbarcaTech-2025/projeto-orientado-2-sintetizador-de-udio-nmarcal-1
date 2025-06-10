
# Projetos de Sistemas Embarcados - EmbarcaTech 2025

Autor: **Nícolas Marçal**

Curso: Residência Tecnológica em Sistemas Embarcados

Instituição: EmbarcaTech - HBr

Campinas, Junho de 2025

---

## Objetivo do projeto

Desenvolver um Sintetizador de Áudio, capaz de gravar e reproduzir áudio, dentro das especificações do sistema – duração do áudio (em segundos), taxa de amostragem, frequência do áudio reproduzido, etc.

---

## Lógica 

Esse projeto envolve três etapas principais:

### 1. Aquisição do Sinal
- **Captura do Áudio:** O microfone capta sons (como voz ou ruídos ambientes).
- **Conversão Analógico-Digital:** O sinal captado é convertido em digital pelo ADC do microcontrolador, sendo a taxa de amostragem essencial para garantir a clareza na gravação.

### 2. Processamento e Armazenamento
- **Armazenamento:** Os dados digitais do áudio são guardados na memória (RAM ou cartão SD).
- **Processamento:** É possível aplicar técnicas básicas, como filtragem para reduzir ruídos, normalização para equalizar níveis e até mesmo uma compressão simples para otimização dos dados.

### 3. Síntese e Reprodução
- **Reconversão do Sinal:** O áudio digital é convertido de volta para analógico para reprodução.
- **Métodos de Reprodução:**
  - **PWM (Modulação de Largura de Pulso):** O microcontrolador gera sinal ajustando o duty cycle para que este se corresponda com os níveis de amplitude das amostras de áudio, podendo ser filtrado (por exemplo, com um filtro RC) para reconstruir a forma de onda original.
  - **I2S com Amplificador Classe D:** Para uma qualidade sonora superior, o áudio é transmitido via I2S e amplificado para ser reproduzido em alto-falantes externos.

### Conceitos Envolvidos
- **Eletrônica e Processamento de Sinais:**  
  Combina conhecimentos de ADC, taxa de amostragem, filtragem, amplificação e técnicas de geração de sinal por PWM/I2S.
- **Integração Prática:**  
  O projeto demonstra como captar, processar e reproduzir áudio, unindo eletrônica, programação, física (ondas sonoras) e matemática (frequências, amostragem e transformadas de sinal).

---

## Componentes:

![componentes_1_synth_audio](https://github.com/EmbarcaTech-2025/projeto-orientado-2-sintetizador-de-udio-nmarcal-1/blob/8a0dfd52aee6009d60af168650bb7425759e68d8/assets/imagem_componentes_1_synth_audio.jpg?raw=true)
![componentes_2_synth_audio](https://github.com/EmbarcaTech-2025/projeto-orientado-2-sintetizador-de-udio-nmarcal-1/blob/ce4ed4efc0b78f8a8ee08ef2b52a4509a78212fb/assets/imagem_componentes_2_synth_audio.jpg?raw=true)

---

## Vídeo:

Vídeo do sistema em uso: [Galton Board - Semana 8 - Embarcatech 2025 2º Fase](https://www.youtube.com/watch?v=iCuJRc8VhTc&feature=youtu.be)


---
## Execução:

1. Abra o projeto no VS Code, usando o ambiente com suporte ao SDK do Raspberry Pi Pico (CMake + compilador ARM);
2. Compile o projeto normalmente (Ctrl+Shift+B no VS Code ou via terminal com cmake e make);
3. Conecte sua BitDogLab via cabo USB e coloque a Pico no modo de boot (pressione o botão BOOTSEL e conecte o cabo);
4. Copie o arquivo .uf2 gerado para a unidade de armazenamento que aparece (RPI-RP2);
5. A Pico reiniciará automaticamente e começará a executar o código;

---

## 📜 Licença
MIT License - MIT GPL-3.0.

---
### sdkVersion 2.1.1

