import os
print "working in",os.getcwd()
print "welcome"
print ">",
s=raw_input()
while s!='exit':
    print "you entered:",s
    print ">",
    s=raw_input()
