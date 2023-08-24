import logging
from logging.handlers import SysLogHandler

sysloghHandlr = SysLogHandler(address=("127.0.0.1", 514), facility=SysLogHandler.LOG_AUTH)

logger = logging.getLogger()
logger.addHandler(sysloghHandlr)
logger.setLevel(logging.INFO)



logger.info("This is a sample info message")
logger.warning("Sample warning message")