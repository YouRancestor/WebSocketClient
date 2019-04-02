import tornado
import tornado.websocket
from tornado.httpserver import HTTPServer
from tornado.ioloop import IOLoop
from tornado.web import Application


class WSHandler(tornado.websocket.WebSocketHandler):
    def __init__(self, application, request, **kwargs):
        super(WSHandler, self).__init__(application, request, **kwargs)

    def open(self):
        self.write_message("Hello")
        print('new connection!')

    def on_message(self, msg):
        self.write_message("you said: "+msg)

    def on_close(self):
        print ("closed")

class HTTPHandler(tornado.web.RequestHandler):
    def __init__(self, application, request, **kwargs):
        super(HTTPHandler, self).__init__(application, request, **kwargs)

    def get(self):
        pass

    def post(self):
        pass

if __name__ == '__main__':
    app = Application([
        (r"/ws", WSHandler),
        (r"/", HTTPHandler)
    ])
    server = HTTPServer(app)
    server.listen(8000)
    IOLoop.current().start()