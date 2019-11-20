var mode = 'null';
var wsQueue = [];
var wsBusy = false;
var wsTimerId;
var ws; // Web Socket

// json with effect definitions
var effectInfo;

// Default modal properties
$.fn.modal.Constructor.DEFAULTS.backdrop = 'static';
$.fn.modal.Constructor.DEFAULTS.keyboard = false;

// jQuery doc ready
$(function() {
    // Menu navigation for single page layout
    $('ul.navbar-nav li a').click(function() {
        // Highlight proper navbar item
        $('.nav li').removeClass('active');
        $(this).parent().addClass('active');

        // Show the proper menu div
        $('.mdiv').addClass('hidden');
        $($(this).attr('href')).removeClass('hidden');

        // kick start the live stream
        if ($(this).attr('href') == "#diag") {
            wsEnqueue('V1');
        }

        // Collapse the menu on smaller screens
        $('#navbar').removeClass('in').attr('aria-expanded', 'false');
        $('.navbar-toggle').attr('aria-expanded', 'false');

        // Firmware selection and upload
        $('#efu').change(function () {
            $('#updatefw').submit();
            $('#update').modal();
        });

        // Set page event feeds
        //feed();
    });

    // Drawing canvas - move to diagnostics
    var canvas = document.getElementById("canvas");
    var ctx = canvas.getContext("2d");
    ctx.font = "20px Arial";
    ctx.textAlign = "center";

    ////////////////////////////////////////////////////
    //
    //  WiFi configuration change callbacks
    //
    ////////////////////////////////////////////////////
    // DHCP field toggles
    $('#network #dhcp').change(function() {
        if ($(this).is(':checked')) {
            $('.dhcp').addClass('hidden');
       } else {
            $('.dhcp').removeClass('hidden');
       }
    });

    // Hostname, SSID, and Password validation
    $('#network #hostname').keyup(function() {
        wifiValidation();
    });
    $('#network #sta_timeout').keyup(function() {
        wifiValidation();
    });
    $('#network #ssid').keyup(function() {
        wifiValidation();
    });
    $('#network #passphrase').keyup(function() {
        wifiValidation();
    });
    $('#network #ap').change(function () {
        wifiValidation();
    });
    $('#network #dhcp').change(function () {
        wifiValidation();
    });
    $('#network #gateway').keyup(function () {
        wifiValidation();
    });
    $('#network #ip').keyup(function () {
        wifiValidation();
    });
    $('#network #netmask').keyup(function () {
        wifiValidation();
    });

    ////////////////////////////////////////////////////
    //
    //  I/O module configuration change callbacks
    //
    ////////////////////////////////////////////////////
    $('#config #device #input').change(function () {
        $('#imode').load($(this).val() + ".html", function() {
            wsEnqueue(JSON.stringify({'cmd':{'get':$('#config #device #input').val()}}));
        });
    });
    $('#config #device #output').change(function () {
        $('#omode').load($(this).val() + ".html", function() {
            wsEnqueue(JSON.stringify({'cmd':{'get':$('#config #device #output').val()}}));
        });
    });

    // Autoload tab based on URL hash
    var hash = window.location.hash;
    hash && $('ul.navbar-nav li a[href="' + hash + '"]').click();

});

// Page event feeds
function feed() {
    if ($('#home').is(':visible')) {
        wsEnqueue('XJ');

        setTimeout(function() {
            feed();
        }, 1000);
    }
}

// Param parser
function param(name) {
    return (location.search.split(name + '=')[1] || '').split('&')[0];
}

// WiFi validation routines
function wifiValidation() {
    var WifiSaveDisabled = false;
    var re = /^([a-zA-Z0-9][a-zA-Z0-9][a-zA-Z0-9\-.]*[a-zA-Z0-9.])$/;
    if (re.test($('#network #hostname').val()) && $('#network #hostname').val().length <= 255) {
        $('#network #fg_hostname').removeClass('has-error');
    } else {
        $('#network #fg_hostname').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#network #sta_timeout').val() >= 5) {
        $('#network #fg_staTimeout').removeClass('has-error');
    } else {
        $('#network #fg_staTimeout').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#network #ssid').val().length <= 32) {
        $('#network #fg_ssid').removeClass('has-error');
    } else {
        $('#network #fg_ssid').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#network #passphrase').val().length <= 64) {
        $('#network #fg_password').removeClass('has-error');
    } else {
        $('#network #fg_password').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#network #dhcp').prop('checked') === false) {
        var iptest = new RegExp(''
        + /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.source
        );

        if (iptest.test($('#network #ip').val())) {
            $('#network #fg_ip').removeClass('has-error');
        } else {
            $('#network #fg_ip').addClass('has-error');
            WifiSaveDisabled = true;
        }
        if (iptest.test($('#network #networkmask').val())) {
            $('#network #fg_netmask').removeClass('has-error');
        } else {
            $('#network #fg_netmask').addClass('has-error');
            WifiSaveDisabled = true;
        }
        if (iptest.test($('#network #gateway').val())) {
            $('#network #fg_gateway').removeClass('has-error');
        } else {
            $('#network #fg_gateway').addClass('has-error');
            WifiSaveDisabled = true;
        }
    }
    $('#network #btn_wifi').prop('disabled', WifiSaveDisabled);
}

// Builds jQuery selectors from JSON data and updates the web interface
var selector = [];
function updateFromJSON(obj) {
    for (var k in obj) {
        selector.push('#' + k);
        if (typeof obj[k] == 'object' && obj[k] !== null) {
            updateFromJSON(obj[k]);
        } else {
            var jqSelector = selector.join(' ');
            if (typeof obj[k] === 'boolean') {
                $(jqSelector).prop('checked', obj[k]);
            } else {
                $(jqSelector).val(obj[k]);
            }
            // Trigger keyup / change events
            $(jqSelector).trigger('keyup');
            $(jqSelector).trigger('change');
        }
        selector.pop();
    }
}

// Builds jQuery selectors from JSON data and populates select options
function addOptionDataFromJSON(obj) {
    for (var k in obj) {
        selector.push('#' + k);
        if (typeof obj[k] == 'object' && obj[k] !== null) {
            addOptionDataFromJSON(obj[k]);
        } else {
            // Get the key and remove the json array index
            var key = selector.pop().substr(1);
            selector.pop();
            var jqSelector = selector.join(' ');
            selector.push('','');
            $(jqSelector).append(
                '<option value="' + key + '">' + obj[k] + '</option>');
        }
        selector.pop();
    }
}

// Buils JSON config submission for "WiFi" tab - Needs updated
function submitWiFiConfig() {
    var ip = $('#ip').val().split('.');
    var netmask = $('#netmask').val().split('.');
    var gateway = $('#gateway').val().split('.');

    var json = {
            'network': {
                'ssid': $('#ssid').val(),
                'passphrase': $('#passphrase').val(),
                'hostname': $('#hostname').val(),
                'sta_timeout': $('#sta_timeout').val(),
                'ip': [parseInt(ip[0]), parseInt(ip[1]), parseInt(ip[2]), parseInt(ip[3])],
                'netmask': [parseInt(netmask[0]), parseInt(netmask[1]), parseInt(netmask[2]), parseInt(netmask[3])],
                'gateway': [parseInt(gateway[0]), parseInt(gateway[1]), parseInt(gateway[2]), parseInt(gateway[3])],
                'dhcp': $('#dhcp').prop('checked'),
                'ap_fallback': $('#ap').prop('checked')
            }
        };
    wsEnqueue('S1' + JSON.stringify(json));
}

// Build dynamic JSON config submission for "Device" tab
function submitDeviceConfig() {
    // Build input mode JSON data for submission
    var input = $('#config #device #input').val();
    var inputJson = {};
    var inputids = $('#config #imode :input').map(function () {
        return $(this).attr('id');
    }).get();

    inputids.forEach(function (id) {
        var select = '#config #imode #' + id;
        if ($(select).is(':checkbox')) {
            inputJson[id] = $(select).prop('checked');
        } else {
            inputJson[id] = $(select).val();
        }
    });

    // Build output mode JSON data for submission
    var output = $('#config #device #output').val();
    var outputJson = {};
        var outputids = $('#config #omode :input').map(function () {
        return $(this).attr('id');
    }).get();

    outputids.forEach(function (id) {
        var select = '#config #omode #' + id;
        if ($(select).is(':checkbox')) {
            outputJson[id] = $(select).prop('checked');
        } else {
            outputJson[id] = $(select).val();
        }
    });

    var json = {
        'device': {
            'id': $('#config #device #id').val(),
            'input': input,
            'output': output
        },

        [input]: inputJson,

        [output]: outputJson
    }

    wsEnqueue(JSON.stringify({'cmd':{'set': json}}));
}

////////////////////////////////////////////////////
//
//  Websocket stuff
//
////////////////////////////////////////////////////
// On websocket connect
function wsConnect() {
    if ('WebSocket' in window) {

// accept ?target=10.0.0.123 to make a WS connection to another device
        var target;
        if (!(target = param('target'))) {
            target = document.location.host;
        }

        // Open a new web socket and set the binary type
        ws = new WebSocket('ws://' + target + '/ws');
        ws.binaryType = 'arraybuffer';

        // When connection is opened, get core data.
        // Module data is loaded in module change / load callbacks
        ws.onopen = function() {
            $('#wserror').modal('hide');
            wsEnqueue(JSON.stringify({'cmd':{'get':'network'}}));   // Get network config
            wsEnqueue(JSON.stringify({'cmd':{'opt':'device'}}));    // Get device option data
            wsEnqueue(JSON.stringify({'cmd':{'get':'device'}}));    // Get device config
            //feed();
        };

        ws.onmessage = function (event) {
            if(typeof event.data === "string") {
                // Process "simple" message format
                if (event.data.startsWith("X")) {
                    var data = event.data.substr(2);
                    getJsonStatus(data);
                // Process as JSON
                } else {
                    var msg = JSON.parse(event.data);
                    // "GET" message is a response to a get request. Populate the frontend.
                    if (msg.hasOwnProperty("get")) {
                        updateFromJSON(msg.get);
                    }

                    // "SET" message is a reponse to a set request. Data has been validated and saved, Populate the frontend.
                    if (msg.hasOwnProperty("set")) {
                        updateFromJSON(msg.set);
                    }

                    // "OPT" message is select option data
                    if (msg.hasOwnProperty("opt")) {
                        addOptionDataFromJSON(msg.opt);
                    }

                }
/*
                var cmd = event.data.substr(0, 2);
                var data = event.data.substr(2);
                switch (cmd) {
                case 'E1':
                    getElements(data);
                    break;
                case 'G1':
                    getConfig(data);
                    break;
                case 'G2':
                    getConfigStatus(data);
                    break;
                case 'G3':
                    getEffectInfo(data);
                    break;
                case 'G4':
                    refreshGamma(data);
                    break;
                case 'S1':
                    setConfig(data);
                    reboot();
                    break;
                case 'S2':
                    setConfig(data);
                    break;
                case 'S3':
                    snackSave();
                    break;
                case 'S4':
                    break;
                case 'XJ':
                    getJsonStatus(data);
                    break;
                case 'X6':
                    showReboot();
                    break;
                default:
                    console.log('Unknown Command: ' + event.data);
                    break;
                }
*/
            } else {
                streamData= new Uint8Array(event.data);
                drawStream(streamData);
                if ($('#diag').is(':visible')) wsEnqueue('V1');
            }
            wsReadyToSend();
        };

        ws.onclose = function() {
            $('#wserror').modal();
            wsConnect();
        };

    } else {
        alert('WebSockets is NOT supported by your Browser! You will need to upgrade your browser or downgrade to v2.0 of the ESPixelStick firmware.');
    }
}

// Websocket message queuer
function wsEnqueue(message) {
    //only add a message to the queue if there isn't already one of the same type already queued, otherwise update the message with the latest request.
//    var wsQueueIndex = wsQueue.findIndex(wsCheckQueue,message);
//    if(wsQueueIndex == -1) {
        //add message
        wsQueue.push(message);
//    } else {
        //update message
//        wsQueue[wsQueueIndex]=message;
//    }
    wsProcessQueue();
}

// Websocket message queuer
function wsCheckQueue(value) {
    //messages are of the same type if the first two characters match
    return value.substr(0,2)==this.substr(0,2);
}

// Websocket message queuer
function wsProcessQueue() {
    //check if currently waiting for a response
    if(wsBusy) {
        //console.log('WS queue busy : ' + wsQueue);
    } else {
        //set wsBusy flag that we are waiting for a response
        wsBusy=true;
        //get next message from queue.
        var message=wsQueue.shift();
        //set timeout to clear flag and try next message if response isn't recieved. Short timeout for message types that don't generate a response.
        if(['T0','T1','T2','T3','T4','T5','T6','T7','X6'].indexOf(message.substr(0,2))) {
            timeout=40;
        } else {
            timeout=2000;
        }
        wsTimerId=setTimeout(wsReadyToSend,timeout);
        //send it.
        //console.log('WS sending ' + message);
        ws.send(message);
    }
}

// Websocket message queuer
function wsReadyToSend() {
    clearTimeout(wsTimerId);
    wsBusy=false;
    if(wsQueue.length>0) {
        //send next message
        wsProcessQueue();
    } else {
        //console.log('WS queue empty');
    }
}

// Move to diagnostics
function drawStream(streamData) {
    var cols=parseInt($('#v_columns').val());
    var size=Math.floor((canvas.width-20)/cols);
    if($("input[name='viewStyle'][value='RGB']").prop('checked')) {
        maxDisplay=Math.min(streamData.length, (cols*Math.floor((canvas.height-30)/size))*3);
        for (i = 0; i < maxDisplay; i+=3) {
            ctx.fillStyle='rgb(' + streamData[i+0] + ',' + streamData[i+1] + ',' + streamData[i+2] + ')';
            var col=(i/3)%cols;
            var row=Math.floor((i/3)/cols);
            ctx.fillRect(10+(col*size),10+(row*size),size-1,size-1);
        }
    } else {
        maxDisplay=Math.min(streamData.length, (cols*Math.floor((canvas.height-30)/size)));
        for (i = 0; i < maxDisplay; i++) {
            ctx.fillStyle='rgb(' + streamData[i] + ',' + streamData[i] + ',' + streamData[i] + ')';
            var col=(i)%cols;
            var row=Math.floor(i/cols);
            ctx.fillRect(10+(col*size),10+(row*size),size-2,size-2);
        }
    }
    if(streamData.length>maxDisplay) {
        ctx.fillStyle='rgb(204,0,0)';
        ctx.fillRect(0,canvas.height-25,canvas.width,25);
        ctx.fillStyle='rgb(255,255,255)';
        ctx.fillText("Increase number of columns to show all data" , (canvas.width/2), canvas.height-5);
    }

}

// Move to diagnostics
function clearStream() {
    if (typeof ctx !== 'undefined') {
     ctx.clearRect(0, 0, canvas.width, canvas.height);
    }
}

// Remove?
function getConfig(data) {
    var config = JSON.parse(data);

    // Device and Network config
    $('#title').text('ESPS - ' + config.device.id);
    $('#name').text(config.device.id);
    $('#deviceid').val(config.device.id);
    $('#ssid').val(config.network.ssid);
    $('#passphrase').val(config.network.passphrase);
    $('#hostname').val(config.network.hostname);
    $('#sta_timeout').val(config.network.sta_timeout);
    $('#dhcp').prop('checked', config.network.dhcp);
    if (config.network.dhcp) {
        $('.dhcp').addClass('hidden');
    } else {
        $('.dhcp').removeClass('hidden');
    }
    $('#ap').prop('checked', config.network.ap_fallback);
    $('#ip').val(config.network.ip[0] + '.' +
            config.network.ip[1] + '.' +
            config.network.ip[2] + '.' +
            config.network.ip[3]);
    $('#netmask').val(config.network.netmask[0] + '.' +
            config.network.netmask[1] + '.' +
            config.network.netmask[2] + '.' +
            config.network.netmask[3]);
    $('#gateway').val(config.network.gateway[0] + '.' +
            config.network.gateway[1] + '.' +
            config.network.gateway[2] + '.' +
            config.network.gateway[3]);

    // MQTT Config
    $('#mqtt').prop('checked', config.mqtt.enabled);
    if (config.mqtt.enabled) {
        $('.mqtt').removeClass('hidden');
    } else {
        $('.mqtt').addClass('hidden');
    }
    $('#mqtt_ip').val(config.mqtt.ip);
    $('#mqtt_port').val(config.mqtt.port);
    $('#mqtt_user').val(config.mqtt.user);
    $('#mqtt_password').val(config.mqtt.password);
    $('#mqtt_topic').val(config.mqtt.topic);
    updateMQTTSet();
    $('#mqtt_haprefix').val(config.mqtt.haprefix);
    $('#mqtt_clean').prop('checked', config.mqtt.clean);
    $('#mqtt_hadisco').prop('checked', config.mqtt.hadisco);
    if (config.mqtt.hadisco) {
        $('#mqtt_haprefix').prop('disabled', false);
    } else {
        $('#mqtt_haprefix').prop('disabled', true);
    }

    // E1.31 Config
    $('#universe').val(config.e131.universe);
    $('#universe_limit').val(config.e131.universe_limit);
    $('#channel_start').val(config.e131.channel_start);
    $('#multicast').prop('checked', config.e131.multicast);

    // Output Config
    $('.odiv').addClass('hidden');
    if (config.device.mode & 0x01) {  // Pixel
        mode = 'pixel';
        $('#o_pixel').removeClass('hidden');
        $('#p_count').val(config.e131.channel_count / 3);
        $('#p_type').val(config.pixel.type);
        $('#p_color').val(config.pixel.color);
        $('#p_groupSize').val(config.pixel.groupSize);
        $('#p_zigSize').val(config.pixel.zigSize);
        $('#p_gammaVal').val(config.pixel.gammaVal);
        $('#p_briteVal').val(config.pixel.briteVal);

//      if(config.e131.channel_count / 3 <8 ) {
//          $('#v_columns').val(config.e131.channel_count / 3);
//      } else if (config.e131.channel_count / 3 <50 ) {
//          $('#v_columns').val(10);
//      } else {
//          $('#v_columns').val(25);
//      }
        $('#v_columns').val(Math.floor(Math.sqrt(config.e131.channel_count/3)));

        $("input[name='viewStyle'][value='RGB']").trigger('click');
        clearStream();

        // Trigger updated elements
        $('#p_type').trigger('click');
        $('#p_count').trigger('change');
    }

    if (config.device.mode & 0x02) {  // Serial
        mode = 'serial';
        $('#o_serial').removeClass('hidden');
        $('#s_count').val(config.e131.channel_count);
        $('#s_proto').val(config.serial.type);
        $('#s_baud').val(config.serial.baudrate);

        if (config.e131.channel_count<=64 ) {
            $('#v_columns').val(8);
        } else {
            $('#v_columns').val(16);
        }
        $("input[name='viewStyle'][value='Channel']").trigger('click');
        clearStream();

        // Trigger updated elements
        $('#s_proto').trigger('click');
        $('#s_count').trigger('change');
    }
}

// Needs updated
function getConfigStatus(data) {
    var status = JSON.parse(data);

//    $('#x_ssid').text(status.ssid);
    $('#x_hostname').text(status.hostname);
    $('#x_ip').text(status.ip);
    $('#x_mac').text(status.mac);
    $('#x_version').text(status.version);
    $('#x_built').text(status.built);
    $('#x_flashchipid').text(status.flashchipid);
    $('#x_usedflashsize').text(status.usedflashsize);
    $('#x_realflashsize').text(status.realflashsize);
    $('#x_freeheap').text(status.freeheap);
}

// Needs updated
function getJsonStatus(data) {
    var status = JSON.parse(data);

    var rssi = +status.system.rssi;
    var quality = 2 * (rssi + 100);

    if (rssi <= -100)
        quality = 0;
    else if (rssi >= -50)
        quality = 100;

    $('#x_rssi').text(rssi);
    $('#x_quality').text(quality);

// getHeap(data)
    $('#x_freeheap').text( status.system.freeheap );

// getUptime
    var date = new Date(+status.system.uptime);
    var str = '';

    str += Math.floor(date.getTime()/86400000) + " days, ";
    str += ("0" + date.getUTCHours()).slice(-2) + ":";
    str += ("0" + date.getUTCMinutes()).slice(-2) + ":";
    str += ("0" + date.getUTCSeconds()).slice(-2);
    $('#x_uptime').text(str);

// getE131Status(data)
    $('#uni_first').text(status.e131.universe);
    $('#uni_last').text(status.e131.uniLast);
    $('#pkts').text(status.e131.num_packets);
    $('#serr').text(status.e131.seq_errors);
    $('#perr').text(status.e131.packet_errors);
    $('#clientip').text(status.e131.last_clientIP);
}

// Show "save" snackbar for 3sec
function snackSave() {
    var x = document.getElementById('snackbar');
    x.className = 'show';
    setTimeout(function(){
        x.className = x.className.replace('show', '');
    }, 3000);
}

// Needs updated
function setConfig() {
    // Get config to refresh UI and show result
    wsEnqueue("G1");
    snackSave();
}

// Show reboot modal
function showReboot() {
    $('#update').modal('hide');
    $('#reboot').modal();
    setTimeout(function() {
        if($('#dhcp').prop('checked')) {
            window.location.assign("/");
        } else {
            window.location.assign("http://" + $('#ip').val());
        }
    }, 5000);
}

// Queue reboot
function reboot() {
    showReboot();
    wsEnqueue('X6');
}
