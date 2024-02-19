import pykd
from sourceline import Inspect

def getProcessByName(name):
    processid = 0
    lower_name = name.lower()
    for (pid, pname, user) in pykd.getLocalProcesses():
        if pname.lower() == lower_name:
            processid = pid
            break
    return processid

def main():
    pid = getProcessByName('producer.exe')
    try:
        a = Inspect(pid)
        lineInfo = a.getLineFromSymName('producer!Person::passVoid')
        print(lineInfo.FileName)
        print(lineInfo)
    except:
        pass

if __name__ == "__main__":
    main()
