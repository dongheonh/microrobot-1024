![Group 110](https://github.com/user-attachments/assets/3c313865-117c-45a5-b102-63f436cded69)

# Firmware
- firmware/pico1/pico1.ino
- firmware/pico2/pico2.ino

## pico1.ino
1. From UART (pico2) recieve 512 electromagnet signal
2. Action1: send action to i2c0 
3. Action2: send action to i2c1

## pico2.ino
1. From USB (PC) recieve 1024 electromagnet signal
2. Send 512 electromagnet signal to pico1
3. Action1: send action to i2c0 
4. Action2: send action to i2c1

## Flash pico1.ino and pico2.ino
