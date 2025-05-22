import os
import subprocess
from time import sleep
import serial.tools.list_ports

def identificar_hardware(portas):
    # Tenta encontrar automaticamente a porta USB do ESP32 ou Arduino
    for porta in portas:
      if 'CP210' in porta.description:
        print("ESP32 identificado na porta ", porta.device)
        return {"hardware":1,"porta":porta.device}
      elif 'CH340' in porta.description:
        print("Arduino identificado na porta ", porta.device)
        return {"hardware":3,"porta":porta.device}
      
    return 0

def escolher_arquivo_ino():
    arquivos_ino = {}
    for pasta_atual, subpastas, arquivos in os.walk(os.getcwd()):
        for arquivo in arquivos:
            if arquivo.endswith('.ino'):
                caminho_completo = os.path.join(pasta_atual, arquivo)
                arquivos_ino[arquivo] = caminho_completo

    if arquivos_ino:
        if len(arquivos_ino) == 1:
            print(f"loader.py: Unico projeto encontrado: {list(arquivos_ino.keys())[0]}")
            return list(arquivos_ino.keys())[0]
        
        print("=" * 60)
        print("Projeto(s) .ino encontrado(s):")
        for i, (nome_arquivo, caminho) in enumerate(arquivos_ino.items()):
            print(f"{i + 1}. {nome_arquivo}: ({caminho})")
        print("=" * 60)
        escolha = int(input("Digite o indice do projeto a ser carregado: "))
        while escolha < 1 or escolha > len(arquivos_ino):
            print("Escolha inválida. Tente novamente.")
            escolha = int(input("Digite o indice do projeto a ser carregado: "))

        return list(arquivos_ino.keys())[escolha - 1]
    else:
        print("loader.py: Nenhum arquivo .ino encontrado nesta pasta e subpastas.")
        exit()

def escolher_porta_usb(portas_disponiveis):
    # Exibir as portas USB disponíveis para o usuário escolher
    print("=" * 60)
    print("Portas USB disponíveis:")
    for i, porta in enumerate(portas_disponiveis):
        print(f"{i + 1}. {porta}")
    print("=" * 60)
    # Pede interação do usuário para escolha da porta
    escolha = int(input("Digite o indice da porta USB que deseja utilizar: "))

    # Verificar se a escolha é válida
    while escolha < 1 or escolha > len(portas_disponiveis):
        print("Escolha inválida. Tente novamente.")
        escolha = int(input("Digite o indice da porta USB que deseja utilizar: "))

    return portas_disponiveis[escolha - 1].device

def carregar_env():
    # Carregar arquivo .env
    try:
        with open('.env', 'r') as env:
            for linha in env:
                if linha.startswith('BUILD'):
                    tipo_build = linha.split('=')[1].strip()
                    print(f"loader.py: Tipo de build: {tipo_build}")
                    return tipo_build
                else:
                    print("loader.py: Arquivo .env não contém a variável BUILD.")
                    return 'none'
    except FileNotFoundError:
        print("loader.py: Arquivo .env não encontrado.")
        return 'none'
    
def escolhe_hardware():
    # Apresentar opções ao usuário
    print("=" * 60)
    print("Qual Hardware deseja gravar?")
    print('Aperte a tecla "1" ESP32')
    print('Aperte a tecla "2" Arduino')
    print('Aperte a tecla "3" Arduino old bootloader')
    print("=" * 60)
    op = input("Digite sua opção: ")
    while op != "1" and op != "2" and op != "3":
        print("Opção inválida. Tente novamente")
        op = input("Digite sua opção: ")
    return int(op)

def arquivos_a_carregar(hardware, arquivo_ino):
    arquivos_build = ['', '', '']

    pasta_build = os.path.join(os.getcwd(), 'build' if hardware == 1 else 'build_arduino')
    if not os.path.exists(pasta_build):
        if hardware == 1:
          print("loader.py: Pasta build não foi encontrada. Encerrando processo.")
        else:
          print("loader.py: Pasta build_arduino não foi encontrada. Encerrando processo.")  
        exit()
    
    if hardware == 1:  # ESP
      arquivos_build[0] = os.path.join(pasta_build, arquivo_ino + '.bootloader.bin')
      arquivos_build[1] = os.path.join(pasta_build, arquivo_ino + '.partitions.bin')
      arquivos_build[2] = os.path.join(pasta_build, arquivo_ino + '.bin')
    else:              # Arduino
      arquivos_build[0] = os.path.join(pasta_build, arquivo_ino + '.hex')
      arquivos_build[1] = os.path.join(pasta_build, arquivo_ino + '.eep')

    for arquivo in arquivos_build:
        if arquivo and not os.path.exists(arquivo):
            print(f"loader.py: O arquivo {arquivo} não foi encontrado.")
            if hardware != 1 and arquivo == arquivos_build[1]:
                print("loader.py: Continuando sem gravação da eeprom...\n")
                arquivos_build[1] = ''
            else:
              exit()

    return arquivos_build

def verificar_versao(usb, tipo_build):
    if tipo_build == 'bluetooth':
        print("loader.py: Não é possível verificar a versão do firmware do tipo Bluetooth.")
    else:
        print("loader.py: Conferindo versão gravada...")
        try:
            device = serial.Serial(usb, timeout=3)
            sleep(5)
            device.write(b'v ')
            device.flush()
            resposta = device.read_until(b"\r\n")
            # print(f"loader.py: Resposta: {resposta}")
            versao = resposta.decode('utf-8', errors='ignore').split().pop()
            print(f"loader.py: Versão {versao}\n")

            print("loader.py: Configurando RFID para cartões Mifare...")
            device.write(b'${"read":0} ')
            device.flush()
            resposta = ""
            resposta = device.read_until()
            if resposta.find(b'"read":false') >= 0:
                # print(f"loader.py: {resposta.decode().strip()}")
                print(f"loader.py: Configuração bem sucedida!\n")
            else:
                print(f"loader.py: Erro ao configurar RFID: {resposta.decode().strip()}\n")
            sleep(1.5)
            device.close()
        except Exception as e:
            print(f"loader.py: Erro ao verificar a versão: {e}\n")

def executar_arquivo_usb(usb, hardware, arquivo_ino):
    arquivos = arquivos_a_carregar(hardware, arquivo_ino)
    # chose an implementation, depending on os
    if os.name == 'nt':  # sys.platform == 'win32':
        if   hardware == 1:  # ESP
            flashCmd = r'"' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\esp32\\tools\\esptool_py\\4.5.1/esptool.exe" --chip esp32 --port ' + usb + r' --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x1000 "' + arquivos[0] + r'" 0x8000 "' + arquivos[1] + r'" 0xe000 "' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\esp32\\hardware\\esp32\\2.0.9/tools/partitions/boot_app0.bin" 0x10000 "' + arquivos[2] + r'"'
        elif hardware == 2:  # New bootloader
            flashCmd = r'"' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\arduino\\tools\\avrdude\\6.3.0-arduino17\\bin\\avrdude.exe" "-C' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\arduino\\tools\\avrdude\\6.3.0-arduino17\\etc\\avrdude.conf" -V -patmega328p -carduino "-P' + usb + r'" -b115200 -D "-Uflash:w:' + arquivos[0] + r':i"' #"-Uflash:w:build_arduino/ME_ALL_IN_ONE.ino.hex:i"
            eepromCmd = r'"' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\arduino\\tools\\avrdude\\6.3.0-arduino17\\bin\\avrdude.exe" "-C' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\arduino\\tools\\avrdude\\6.3.0-arduino17\\etc\\avrdude.conf" -V -patmega328p -carduino "-P' + usb + r'" -b115200 -D "-Ueeprom:w:' + arquivos[1] + r':i"' #"-Ueeprom:w:build_arduino/ME_ALL_IN_ONE.ino.eep:i"
        elif hardware == 3:  # Old bootloader1
            flashCmd = r'"' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\arduino\\tools\\avrdude\\6.3.0-arduino17\\bin\\avrdude.exe" "-C' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\arduino\\tools\\avrdude\\6.3.0-arduino17\\etc\\avrdude.conf" -V -patmega328p -carduino "-P' + usb + r'" -b57600 -D "-Uflash:w:' + arquivos[0] + r':i"' 
            eepromCmd = r'"' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\arduino\\tools\\avrdude\\6.3.0-arduino17\\bin\\avrdude.exe" "-C' + os.path.expanduser('~') + r'\\AppData\\Local\\Arduino15\\packages\\arduino\\tools\\avrdude\\6.3.0-arduino17\\etc\\avrdude.conf" -V -patmega328p -carduino "-P' + usb + r'" -b57600 -D "-Ueeprom:w:' + arquivos[1] + r':i"' 
    elif os.name == 'posix': # sys.plataform == 'Darwin' (macOs)
        if   hardware == 1:  # ESP
            flashCmd = r'"' + os.path.expanduser('~') + r'/Library/Arduino15/packages/esp32/tools/esptool_py/4.5.1/esptool" --chip esp32 --port ' + usb + r' --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x1000 "build/ME_ALL_IN_ONE.ino.bootloader.bin" 0x8000 "build/ME_ALL_IN_ONE.ino.partitions.bin" 0xe000 "' + os.path.expanduser('~') + r'/Library/Arduino15/packages/esp32/hardware/esp32/2.0.9/tools/partitions/boot_app0.bin" 0x10000 "build/ME_ALL_IN_ONE.ino.bin"'
        elif hardware == 2:  # New bootloader
            flashCmd = r'"' + os.path.expanduser('~') + r'/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17/bin/avrdude" "-C' + os.path.expanduser('~') + r'/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17/etc/avrdude.conf" -v -V -patmega328p -carduino "-P' + usb + r'" -b115200 -D "-Uflash:w:build_arduino/ME_ALL_IN_ONE.ino.hex:i"'
            eepromCmd = r'"' + os.path.expanduser('~') + r'/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17/bin/avrdude" "-C' + os.path.expanduser('~') + r'/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17/etc/avrdude.conf" -v -V -patmega328p -carduino "-P' + usb + r'" -b115200 -D "-Ueeprom:w:build_arduino/ME_ALL_IN_ONE.ino.hex:i"'
        elif hardware == 3:  # Old bootloader
            flashCmd = r'"' + os.path.expanduser('~') + r'/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17/bin/avrdude" "-C' + os.path.expanduser('~') + r'/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17/etc/avrdude.conf" -v -V -patmega328p -carduino "-P' + usb + r'" -b57600 -D "-Uflash:w:build_arduino/ME_ALL_IN_ONE.ino.hex:i"'
            eepromCmd = r'"' + os.path.expanduser('~') + r'/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17/bin/avrdude" "-C' + os.path.expanduser('~') + r'/Library/Arduino15/packages/arduino/tools/avrdude/6.3.0-arduino17/etc/avrdude.conf" -v -V -patmega328p -carduino "-P' + usb + r'" -b57600 -D "-Ueeprom:w:build_arduino/ME_ALL_IN_ONE.ino.hex:i"'

    try:
        # if hardware != 1:
            # ser = serial.Serial(usb, 9600)
            # ser.close()
            # print("\nloader.py: Porta serial configurada.")
        print("loader.py: Gravando arquivo na memória flash...\n")
        processo = subprocess.Popen(flashCmd)
        processo.communicate(timeout=15 if hardware != 1 else 45)
        if hardware == 2 or hardware == 3 and arquivos[1] != '':
            subprocess.run(eepromCmd)
    except subprocess.TimeoutExpired:
        processo.kill()
        print("\nloader.py: Tempo de gravação expirado.")
        if hardware == 3: 
            print("\nloader.py: Tentando old bootloader.")
            executar_arquivo_usb(usb, 2, arquivo_ino)
        elif hardware == 2:
            print("\nloader.py: Tentando outro bootloader.")
            executar_arquivo_usb(usb, 3, arquivo_ino)
    except Exception as e:
        print(f"loader.py: Erro ao executar o arquivo: {e}")

    # print("loader.py: Conferindo versão gravada...")
    # device = serial.Serial(usb)
    # sleep(1.5)
    # device.write(b'v ')
    # device.flush()
    # resposta = device.readline()
    # resposta = resposta.decode().strip()
    # print(f"loader.py: Versão {resposta}\n")
    # device.close()

# Início do programa
# Carrega arquivo .env
tipo_build = carregar_env();      
# Busca portas COM disponíveis
portas_disponiveis = serial.tools.list_ports.comports(True)
# Buscar arquivos .ino na pasta atual
arquivo_ino = escolher_arquivo_ino()  
# Identificar o hardware conectado
# identificador = identificar_hardware(portas_disponiveis)
identificador = 0 # Workarround para não identificar automaticamente
# Se não foi possível identificar automaticamente, perguntar ao usuário
if identificador == 0:
  # Listar as portas USB disponíveis e pedir para o usuário escolher qual usar
  porta_usb = escolher_porta_usb(portas_disponiveis)
  # Pergunta ao usuário qual o hardware de destino
  hardware = escolhe_hardware()
else:
  porta_usb = identificador["porta"]
  hardware = identificador["hardware"]
# Executar o arquivo na porta USB selecionada
executar_arquivo_usb(porta_usb, hardware, arquivo_ino)
# verificar_versao(porta_usb, tipo_build)

while True:
    # Apresentar opções ao usuário
    print("=" * 60)
    print("Aperte ENTER para regravar.")
    print('Aperte a tecla "1" para alterar a porta COM.')
    print('Aperte a tecla "2" para alterar o hardware.')
    print('Aperte a tecla "3" para alterar o projeto')
    print('Aperte a tecla "4" para verificar a versão gravada.')
    print('Aperte a tecla "0" para finalizar o processo.')
    print("=" * 60)
    opcao = input("Digite sua opção: ")
    
    if opcao == "":
        executar_arquivo_usb(porta_usb, hardware, arquivo_ino)
        verificar_versao(porta_usb, tipo_build)
    elif opcao == "1":
        portas_disponiveis = serial.tools.list_ports.comports(True)
        porta_usb = escolher_porta_usb(portas_disponiveis)
        executar_arquivo_usb(porta_usb, hardware, arquivo_ino)
        verificar_versao(porta_usb, tipo_build)
    elif opcao == "2":
        hardware = escolhe_hardware()
        portas_disponiveis = serial.tools.list_ports.comports(True)
        porta_usb = escolher_porta_usb(portas_disponiveis)
        executar_arquivo_usb(porta_usb, hardware, arquivo_ino)
        verificar_versao(porta_usb, tipo_build)
    elif opcao == "3":
        arquivo_ino = escolher_arquivo_ino()
        executar_arquivo_usb(porta_usb, hardware, arquivo_ino)
        verificar_versao(porta_usb, tipo_build)
    elif opcao == "4":
        verificar_versao(porta_usb, tipo_build)
    elif opcao == "0":
        print("\nloader.py: Finalizando execução.\n")
        break  # Finalizar a execução
    else:
        print("\nloader.py: Opção inválida. Finalizando execução.\n")
        break
