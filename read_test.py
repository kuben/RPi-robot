import RPi.GPIO as GPIO           # import RPi.GPIO module  
import time 

last_edge = time.perf_counter()
read_freq = 0

def handle(pin):
    global last_edge
    global read_freq
    now = time.perf_counter()
    read_freq = 1/(now-last_edge)
    last_edge = now


GPIO.setmode(GPIO.BCM)            # choose BCM or BOARD  
#GPIO.setup(port_or_pin, GPIO.IN)  # set a port/pin as an input  
#GPIO.setup(22, GPIO.OUT) # set a port/pin as an output   
GPIO.setup(4, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)  
#GPIO.output(22, 1)        
GPIO.add_event_detect(4, GPIO.RISING, handle)
try:  
    while True:            # this will carry on until you hit CTRL+C  
#        if GPIO.input(4): # if port 25 == 1  
#            print("Port 4 is 1/HIGH/True - LED ON")
#        else:  
#            print("Port 4 is 0/LOW/False - LED OFF")  
        print('Frequency {}'.format(read_freq))
        time.sleep(0.1)         # wait 0.1 seconds  
  
finally:                   # this block will run no matter how the try block exits  
    GPIO.cleanup()         # clean up after yourself  
