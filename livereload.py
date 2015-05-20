import sys
import time
import logging
from watchdog.observers import Observer
from watchdog.events import LoggingEventHandler
import subprocess

class Rebuilder(LoggingEventHandler):
    def on_moved(self, event):
        pass

    def on_created(self, event):
        pass

    def on_deleted(self, event):
        pass

    def on_modified(self, event):
        super(LoggingEventHandler, self).on_modified(event)
        what = 'directory' if event.is_directory else 'file'
        if(what == "file" and (event.src_path.endswith(".c") or event.src_path.endswith(".h"))):
            print "\n\n"
            logging.info("Modified %s: %s", what, event.src_path)
            subprocess.call(["make","-B"])
            
        

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO,
                        format='%(asctime)s - %(message)s',
                        datefmt='%Y-%m-%d %H:%M:%S')
    path = sys.argv[1] if len(sys.argv) > 1 else '.'
    event_handler = Rebuilder() #LoggingEventHandler()
    observer = Observer()
    observer.schedule(event_handler, path, recursive=True)
    observer.start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        observer.stop()
    observer.join()