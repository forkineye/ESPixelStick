function microAjax(B, A) {
    this.bindFunction = function(E, D) {
        return function() {
            return E.apply(D, [D]);
        };
    };
    this.stateChange = function(D) {
        if (this.request.readyState == 4) {
            this.callbackFunction(this.request.responseText);
        }
    };
    this.getRequest = function() {
        if (window.ActiveXObject) {
            return new ActiveXObject("Microsoft.XMLHTTP");
        } else {
            if (window.XMLHttpRequest) {
                return new XMLHttpRequest();
            }
        }
        return false;
    };
    this.postBody = (arguments[2] || "");
    this.callbackFunction = A;
    this.url = B;
    this.request = this.getRequest();
    if (this.request) {
        var C = this.request;
        C.onreadystatechange = this.bindFunction(this.stateChange, this);
        if (this.postBody !== "") {
            C.open("POST", B, true);
            C.setRequestHeader("X-Requested-With", "XMLHttpRequest");
            C.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
            C.setRequestHeader("Connection", "close");
        } else {
            C.open("GET", B, true);
        }
        C.send(this.postBody);
    }
}

function setValues(url) {
    microAjax(url, function(res) {
        res.split(String.fromCharCode(10)).forEach(function(entry) {
            fields = entry.split("|");
            if (fields[1] == "input") {
                document.getElementById(fields[0]).value = fields[2];
            } else if (fields[1] == "div") {
                document.getElementById(fields[0]).innerHTML = fields[2];
            } else if (fields[1] == "chk") {
                document.getElementById(fields[0]).checked = fields[2];
            } else if (fields[1] == "opt") {
                var opt = document.createElement('option');
                opt.text = fields[2];
                opt.value = fields[3];
                document.getElementById(fields[0]).add(opt);
            }
        });
    });
}
