import threading
import time
import RPi.GPIO as GPIO

GPIO.setmode(GPIO.BOARD)
GPIO.setup(11,GPIO.OUT)
class ThreadingExample(object):

    def __init__(self, period=0):
        self.period = period
        self.running = True

        thread = threading.Thread(target=self.run, args=())
        thread.daemon = True                            # Daemonize thread
        thread.start()                                  # Start the execution

    def run(self):
        """ Method that runs forever """
        t = time.perf_counter()
        while self.running:
            t += self.period/2
            #Half period
            if self.period > 0:
                GPIO.output(11,True)
                time.sleep(max(0,t-time.perf_counter()))

            t += self.period/2
            #Half period
            GPIO.output(11,False)
            time.sleep(max(0,t-time.perf_counter()))
    def stop(self):
        self.running = False
def quit():
    pin.stop()
    GPIO.cleanup()
    print('Bye')

v_max = 5.94
f_max = 7659.57
pin = ThreadingExample()
freq = 0
while freq >= 0:
    if freq == 0:
        pin.period = 0#Special case 0 means Inf
    else:
        pin.period = 1/freq
    try:
        volt = float(input('Voltage (Max {}): '.format(v_max)))
        if volt > v_max:
            volt = v_max
        freq = volt*f_max/v_max
        print('Frequency set to {:.2f}Hz'.format(freq))
    except ValueError:
        print("Invalid number")

quit()
