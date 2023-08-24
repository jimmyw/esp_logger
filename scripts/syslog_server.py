## Minimal Syslog Server in Python.
import logging
import socketserver

LOG_FILE_PATH = 'logs.log'
HOST, PORT = "0.0.0.0", 514

logging.basicConfig(level=logging.INFO, format='%(message)s', filename=LOG_FILE_PATH, filemode='a')

class SyslogUDPHandler(socketserver.BaseRequestHandler):
	def handle(self):
		data = bytes.decode(self.request[0])
		# socket = self.request[1]
		print( "%s: '%s'" % (self.client_address[0], str(data)))
		logging.info(str(data))

if __name__ == "__main__":
	try:
		server = socketserver.UDPServer((HOST,PORT), SyslogUDPHandler)
		server.serve_forever(poll_interval=0.5)
	except (IOError, SystemExit):
		raise
	except KeyboardInterrupt:
		print ("Crtl+C Pressed. Shutting down.")