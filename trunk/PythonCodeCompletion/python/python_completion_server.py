import code
import sys
import os.path
import time
import types
import stdlib_parser
from SimpleXMLRPCServer import SimpleXMLRPCServer
from pysmell import idehelper
from pysmell import tags
from re import split

type_map = {
str(types.ModuleType) : 1,
str(types.ClassType) : 2,
str(types.ObjectType) : 3,
str(types.TypeType) : 4,
str(types.InstanceType) : 5,
str(types.FunctionType) : 6,
str(types.MethodType) : 7,
str(types.UnboundMethodType) : 8,
str(types.BuiltinFunctionType) : 6,
str(types.BuiltinMethodType) : 7,
str(types.LambdaType) : 9,
str(types.GeneratorType) : 10 }

def type_suffix(symbols,s):
    type_str=symbols[s][0]
    if type_str in type_map:
        return '?'+str(type_map[type_str])
    return ''

def _uniquify(l):
    found = set()
    for item in l:
        if item not in found:
            yield item
        found.add(item)


class AsyncServer:
    def __init__(self,port):
        # Create XMLRPC server
        self.timeout=0.2
        self._quit=False
        self.port=port
        self.std_lib=None
    def run(self):
        self.server = SimpleXMLRPCServer(("localhost", self.port))
        self.server.logRequests=0
        self.server.register_introspection_functions()
        #self.server.socket.settimeout(self.timeout) ##timeouts cause probs
        self.server.register_function(self.end,'end')
        self.server.register_function(self.get_completions,'get_completions')
        self.server.register_function(self.tag_gen,'tag_gen')
        self.server.register_function(self.load_stdlib,'load_stdlib')
        self.server.register_function(self.create_stdlib,'create_stdlib')
        self.server.register_function(self.complete_phrase,'complete_phrase')
        while not self._quit:
            self.server.handle_request()
    def load_stdlib(self,path):
        print 'loading lib',path
        self.stdlib=stdlib_parser.load(path)
        return self.stdlib!=None
    def create_stdlib(self,path):
        print 'creating',path
        self.stdlib=stdlib_parser.create(path)
        return self.stdlib!=None
    def complete_phrase(self,phrase):
        print 'complete phrase',phrase
        context=[s.strip() for s in phrase.split('.')]
        phrase=context.pop()
        psymbols=self.stdlib
        for s in context:
            if s not in psymbols:
                print 'x',[]
                return []
            psymbols=psymbols[s][-1]
        if type(psymbols)!=types.DictType:
            return []
        completions=[]
        try:
            for s in psymbols:
                if s.startswith(phrase):
                    completions.append(s)
            completions=sorted(completions)
            print 'COMPS',completions
            completions=[s+type_suffix(psymbols,s) for s in completions]
            return completions
        except:
            import traceback,sys
            traceback.print_exception(*sys.exc_info())
    def tool_tip(self,function):
        return False
    def tag_gen(self, projectDir):
        args = ['pysmell', projectDir, '-o', os.path.join(projectDir, 'PYSMELLTAGS')]
        sys.argv = args
        tags.main()
        return True
    def get_completions(self, cur_file, source, line_no, cur_col):
        """arguments: fullPath, origSource, lineNo, origCol, matcher
            When visiting the file at fullPath, with edited source origSource, find a list
            of possible completion strings for the symbol located at origCol on orgLineNo using
            matching mode matcher"""
        print cur_file,line_no,cur_col
        if not cur_file:
            write('No filename - is the file saved?')
            return TOOLTIP
        source = file(cur_file,'r').read()

        PYSMELLDICT = idehelper.findPYSMELLDICT(cur_file)
        if PYSMELLDICT is None:
            write('No PYSMELLTAGS found - you have to generate one.')
            return TOOLTIP
        print 'found dict'
        line = source.splitlines()[line_no - 1]
        index = idehelper.findBase(line, cur_col)
        base = line[index:cur_col]

        options = idehelper.detectCompletionType(cur_file, source, line_no, cur_col, base, PYSMELLDICT)
        completions = idehelper.findCompletions(base, PYSMELLDICT, options)
        return completions
    def end(self):
        self._quit=True
        return True

def cmd_err():
    print 'Correct usage: python_completion_server.py <port>'
    print '<port> must be a positive integer'
    exit()

if __name__=='__main__':
    if len(sys.argv)!=2:
        port=8000
    else:
        try:
            port = int(sys.argv[1])
        except:
            cmd_err()
        if port<=0:
            cmd_err()

    server=AsyncServer(port)
    server.run()
