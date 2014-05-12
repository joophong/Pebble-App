/* author: John Lewis, Steven Parad, and Joopyo Hong */
 
var count = 0;


Pebble.addEventListener("appmessage",
  function(e) {
    if (e.payload) {
      if (e.payload.fahrenheit) {
        sendToServer(0);
      } else if (e.payload.celsius) {
        sendToServer(1);
      } else if (e.payload.pause) {
        sendToServer(2);
      } else if (e.payload.resume) {
        sendToServer(3);
      } else if (e.payload.time) {
        sendToServer(4);
      } else if (e.payload.graph) {
        sendToServer(5);
      } else if (e.payload.dot) {
        sendToServer(6);
      } else if (e.payload.dash) {
        sendToServer(7);
      }
    }
  }
);

function sendToServer(mode) {

	var req = new XMLHttpRequest();
	var ipAddress = "158.130.63.9"; // Hard coded IP address
	var port = "3001"; // Same port specified as argument to server
	var url = "http://" + ipAddress + ":" + port + "/";
	var method = "GET";
	var async = true;

	req.onload = function(e) {
                // see what came back
                var msg = "Could not connect to server.";
                var response = JSON.parse(req.responseText);
                if (response) {
                    if (response.msg) {
                        msg = response.msg;
                    }
                    else msg = "no msg";
                }
                // sends message back to pebble
                Pebble.sendAppMessage({ "0": msg });
	};

        req.open(method, url + mode, async);
        req.send(null);
  
  if (req.readyState != 4 || req.status != 200) {
      count++;
    if (count > 2) {
      Pebble.sendAppMessage({ "0": "Could not connect to server."});
      count = 0;
    }  
  }
