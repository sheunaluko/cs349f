import os 


def debug(msg) : 
    if True  : 
        print(msg) 
        
        
        
def check_for_file(fname)  : 
    import os.path
    return os.path.isfile(fname) 


def append_to_file(fname, strang) : 
    if not check_for_file(fname) : 
        mode = 'w' 
    else : 
        mode = 'a+' 

    with open(fname, mode) as outfile : 
        outfile.write(strang)
        
        
def logfile(logname, strang) : 
    fname = logname 
    #can add a log dir here if we want
    #fname = "logs/" + logname 
    append_to_file(fname,strang + "\n")