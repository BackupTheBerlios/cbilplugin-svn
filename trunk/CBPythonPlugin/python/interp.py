import code
import sys
import threading
from SimpleXMLRPCServer import SimpleXMLRPCServer

class datastore:
    def __init__(self,lock):
        self.data=''
        self.lock=lock
    def write(self,textstr):
        #queue stdout string to be sent to client periodically
        #print >>sys.__stdout__, 'INTERCEPTED:',textstr
        self.lock.acquire()
        self.data=self.data+textstr
        self.lock.release()
    def flush(self):
        self.lock.acquire()
        self.data=''
        self.lock.release()
    def read(self):
        data=self.data
        self.lock.acquire()
        self.data=''
        self.lock.release()
        return data

class PyInterp (code.InteractiveInterpreter):
    def __init__(self,lock):
        code.InteractiveInterpreter.__init__(self)
        self._stdout=datastore(lock)
        self._stdin=datastore(lock)
        self._stderr=datastore(lock)
        self._running=True
        self._runningeval=False
        self.lock=lock
        self.eval_str=''
    def queue_code(self,eval_str):
        if not self._runningeval:
            self.lock.acquire()
            self.eval_str=eval_str
            self._runningeval=True
            self.lock.notify()
            self.lock.release()
            print >>sys.__stdout__,'queue lock released'
            return True
        else:
            return False
    def main_loop(self):
        while self._running: #runs the eval_str queued by the server, then waits for the next until the server requests exit
            try:
                if self.eval_str!='':
                    print >>sys.__stdout__,'running code',self.eval_str
                    #print self.eval_str
                    try:
                        self.runsource(self.eval_str+'\n')
                        print >>sys.__stdout__,'ran code'
                    except KeyboardInterrupt:
                        print 'Keyboard Interrupt'
                    except:
                        print "error in eval_str", sys.exc_info()[0]
                self.lock.acquire() #acquire the lock to reset eval string and running status
                self._runningeval=False
                self.eval_str=''
                self.lock.notify() #notify the server in case it is waiting
                self.lock.wait() #now await the next instruction
                self.lock.release()
            except KeyboardInterrupt:
                print >>sys.__stdout__,'keyboard interrupt',self.eval_str

class AsyncServer(threading.Thread):
    def __init__(self,port):
        # Create XMLRPC server
        threading.Thread.__init__(self)
        self.lock=threading.Condition(threading.Lock())
        self.timeout=0.2
        self._quit=False
        self.port=port
        self.interp = PyInterp(self.lock)
        sys.stdout=self.interp._stdout
        sys.stdin=self.interp._stdin
        sys.stderr=self.interp._stderr
    def start_interp(self):
        self.interp.main_loop()
    def run(self):
        self.server = SimpleXMLRPCServer(("localhost", self.port))
        self.server.logRequests=0
        self.server.register_introspection_functions()
        #self.server.socket.settimeout(self.timeout)
        self.server.register_function(self.end,'end')
        self.server.register_function(self.run_code,'run_code')
        self.server.register_function(self.break_code,'break_code')
        self.server.register_function(self.cont,'cont')
        while not self._quit:
            self.server.handle_request()
    def end(self):
        if self.interp._runningeval:
            raise KeyboardInterrupt
        self.lock.acquire()
        self.interp._running=False
        self._quit=True
        self.lock.notify()
        self.lock.release()
        return "Session Terminated"
    def break_code(self):
        print >>sys.__stdout__,'break requested'
        if self.interp._runningeval:
            print >>sys.__stdout__,'break request pending'
            try:
                raise KeyboardInterrupt
            except KeyboardInterrupt:
                print >>sys.__stdout__,'exception caught in server thread'
                return True
            print 'exception not caught in server thread'
            return True
        return False
    def run_code(self,eval_str,stdin):
        print >>sys.__stdout__,"compiling code"
        try:
            cobj=code.compile_command(eval_str)
        except:
            print >>sys.__stderr__,"syntax error"
            return -1,'',''
        if cobj==None:
            print >>sys.__stderr__,"statement incomplete"
            return -2,'',''
        print >>sys.__stdout__,"running code"
        if self.interp.queue_code(eval_str):
            return self.cont(stdin)
        else:
            return -3,self.interp._stdin.read(),self.interp._stderr.read()
    def cont(self,stdin):
        self.interp._stdin.write(stdin)
        self.lock.acquire()
        if self.interp._runningeval:
            self.lock.wait(self.timeout)
        self.lock.release()
        return int(self.interp._runningeval),self.interp._stdout.read(),self.interp._stderr.read()
        #return status, stdout, stderr

def cmd_err():
    print 'Correct usage: pyinterp.py <port>'
    print '<port> must be a positive integer'
    exit()

if len(sys.argv)!=2:
    cmd_err()
try:
    port = int(sys.argv[1])
except:
    cmd_err()
if port<=0:
    cmd_err()

print 'starting server on port', port

interp_server=AsyncServer(port)
interp_server.start()
interp_server.start_interp()

print 'server terminated'
