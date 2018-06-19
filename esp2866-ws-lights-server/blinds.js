var cversion = "{VERSION}";
var wsUri = "ws://" + location.host + ":81/";
var repo = "esp8266blinds";
window.fn = {};
window.fn.open = function() {
  var menu = document.getElementById("menu");
  menu.open();
};
window.fn.load = function(page) {
  var content = document.getElementById("content");
  var menu = document.getElementById("menu");
  content
    .load(page)
    .then(menu.close.bind(menu))
    .then(setActions());
};
var gotoPos = function(percent) {
  doSend(percent);
};
var instr = function(action) {
  doSend("(" + action + ")");
};
var setActions = function() {
  doSend("(update)");
  $.get("https://api.github.com/repos/xurban42/" + repo + "/releases", function(
    data
  ) {
    if (data.length > 0 && data[0].tag_name !== cversion) {
      $("#cversion").text(cversion);
      $("#nversion").text(data[0].tag_name);
      $("#update-card").show();
    }
  });
  setTimeout(function() {
    $("#arrow-close").on("click", function() {
      $("#setrange").val(100);
      gotoPos(100);
    });
    $("#arrow-open").on("click", function() {
      $("#setrange").val(0);
      gotoPos(0);
    });
    $("#setrange").on("change", function() {
      gotoPos($("#setrange").val());
    });
    $("#arrow-up-man").on("click", function() {
      instr("-1");
    });
    $("#arrow-down-man").on("click", function() {
      instr("1");
    });
    $("#arrow-stop-man").on("click", function() {
      instr("0");
    });
    $("#set-start").on("click", function() {
      instr("start");
    });
    $("#set-max").on("click", function() {
      instr("max");
    });
  }, 200);
};
$(document).ready(function() {
  setActions();
});
var websocket;
var timeOut;
function retry() {
  clearTimeout(timeOut);
  timeOut = setTimeout(function() {
    websocket = null;
    init();
  }, 5000);
}
function init() {
  ons.notification.toast({ message: "Connecting...", timeout: 1000 });
  try {
    websocket = new WebSocket(wsUri);
    websocket.onclose = function() {};
    websocket.onerror = function(evt) {
      ons.notification.toast({
        message: "Cannot connect to device",
        timeout: 2000
      });
      retry();
    };
    websocket.onopen = function(evt) {
      ons.notification.toast({ message: "Connected to device", timeout: 2000 });
      setTimeout(function() {
        doSend("(update)");
      }, 1000);
    };
    websocket.onclose = function(evt) {
      ons.notification.toast({
        message: "Disconnected. Retrying",
        timeout: 2000
      });
      retry();
    };
    websocket.onmessage = function(evt) {
      try {
        var msg = JSON.parse(evt.data);
        if (typeof msg.position !== "undefined") {
          $("#pbar").attr("value", msg.position);
        }
        if (typeof msg.set !== "undefined") {
          $("#setrange").val(msg.set);
        }
      } catch (err) {}
    };
  } catch (e) {
    ons.notification.toast({
      message: "Cannot connect to device. Retrying...",
      timeout: 2000
    });
    retry();
  }
}
function doSend(msg) {
  if (websocket && websocket.readyState == 1) {
    websocket.send(msg);
  }
}
window.addEventListener("load", init, false);
window.onbeforeunload = function() {
  if (websocket && websocket.readyState == 1) {
    websocket.close();
  }
};
