var mode = 'null';
var wsOutputQueue = [];
var wsBusy = false;
var wsTimerId = null;
var ws = null; // Web Socket

// json with effect definitions
var effectInfo = null;

// global data
var ParsedJsonStatus = null;
var ParsedJsonConfig = null;
var om_config = null; // Output Manager configuration record
var im_config = null; // Input Manager configuration record
var selector = [];
var StatusUpdateRequestTimer = null;

// Default modal properties
$.fn.modal.Constructor.DEFAULTS.backdrop = 'static';
$.fn.modal.Constructor.DEFAULTS.keyboard = false;

// jQuery doc ready
$(function ()
{
    // Menu navigation for single page layout
    $('ul.navbar-nav li a').click(function ()
    {
        // Highlight proper navbar item
        $('.nav li').removeClass('active');
        $(this).parent().addClass('active');

        // Show the proper menu div
        $('.mdiv').addClass('hidden');
        $($(this).attr('href')).removeClass('hidden');

        // kick start the live stream
        if ($(this).attr('href') === "#diag")
        {
            wsEnqueue('V1');
        }

        Console.log("href = " + $(this).attr('href'));

        if ($(this).attr('href') === "#admin") {
            wsEnqueue('XA');
        }

        // kick start the live stream
        if ($(this).attr('href') === "#config")
        {
            wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'output' } })); // Get output config
            wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'input'  } }));  // Get input config
            wsEnqueue(JSON.stringify({ 'cmd': { 'opt': 'device' } })); // Get device option data
        }

        // Collapse the menu on smaller screens
        $('#navbar').removeClass('in').attr('aria-expanded', 'false');
        $('.navbar-toggle').attr('aria-expanded', 'false');

        // Firmware selection and upload
        $('#efu').change(function ()
        {
            $('#updatefw').submit();
            $('#update').modal();
        });
    });

    // Drawing canvas - move to diagnostics
    var canvas = document.getElementById("canvas");
    var ctx = canvas.getContext("2d");
    ctx.font = "20px Arial";
    ctx.textAlign = "center";

    SetUpWifiValidationHandlers();
    SetUpIoChangeHandlers();

    // Autoload tab based on URL hash
    var hash = window.location.hash;
    hash && $('ul.navbar-nav li a[href="' + hash + '"]').click();

    // start updating stats
    RequestStatusUpdate();

});

function SetUpIoChangeHandlers()
{
    ////////////////////////////////////////////////////
    //
    //  I/O module configuration change callbacks
    //
    ////////////////////////////////////////////////////

    // define the input select list action
    $('#config #device #input').change(function () {
        if ($(this).val()) {
            LoadInputConfigurationForm();
        }
    });

    // define the output select list action
    $('#config #device #output').change(function () {
        if ($(this).val()) {
            LoadOutputConfigurationForm();
        }
    });

} // SetUpIoChangeHandlers

function SetUpWifiValidationHandlers()
{
    ////////////////////////////////////////////////////
    //
    //  WiFi configuration change callbacks
    //
    ////////////////////////////////////////////////////
    // DHCP field toggles
    $('#network #dhcp').change(function () {
        if ($(this).is(':checked')) {
            $('.dhcp').addClass('hidden');
        }
        else {
            $('.dhcp').removeClass('hidden');
        }
    });
    $('#network #hostname').keyup(function () {
        wifiValidation();
    });
    $('#network #sta_timeout').keyup(function () {
        wifiValidation();
    });
    $('#network #ssid').keyup(function () {
        wifiValidation();
    });
    $('#network #passphrase').keyup(function () {
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
} // SetUpWifiValidationHandlers

function LoadInputConfigurationForm()
{
    $('#refresh').html('0 ms / 0 fps');

    var filename = $("#config #device #input option:selected").text().toLowerCase() + ".html";
    $('#imode').load(filename, function ()
    {
        ProcessInputModeConfiguration();
    });
} // LoadInputConfigurationForm

function LoadOutputConfigurationForm()
{
    $('#refresh').html('0 ms / 0 fps');

    $('#omode').load($("#config #device #output option:selected").text().toLowerCase() + ".html", function ()
    {
        ProcessOutputModeConfiguration();
    });
} // LoadOutputConfigurationForm

function RequestStatusUpdate()
{
    // is the timer running?
    if (null == StatusUpdateRequestTimer)
    {
        // timer runs forever
        StatusUpdateRequestTimer = setTimeout(function ()
        {
            clearTimeout(StatusUpdateRequestTimer);
            StatusUpdateRequestTimer = null;

            RequestStatusUpdate();

        }, 1000);
    } // end timer was not running

    if ($('#home').is(':visible'))
    {
        // ask for a status update from the server
        wsEnqueue('XJ');
    } // end home (aka status) is visible

} // RequestStatusUpdate

function ParseParameter(name)
{
    return (location.search.split(name + '=')[1] || '').split('&')[0];
}

function wifiValidation()
{
    var WifiSaveDisabled = false;
    var re = /^([a-zA-Z0-9][a-zA-Z0-9][a-zA-Z0-9\-.]*[a-zA-Z0-9.])$/;

    if (re.test($('#network #hostname').val()) && $('#network #hostname').val().length <= 255)
    {
        $('#network #fg_hostname').removeClass('has-error');
    }
    else
    {
        $('#network #fg_hostname').addClass('has-error');
        WifiSaveDisabled = true;
    }

    if ($('#network #sta_timeout').val() >= 5)
    {
        $('#network #fg_staTimeout').removeClass('has-error');
    } else
    {
        $('#network #fg_staTimeout').addClass('has-error');
        WifiSaveDisabled = true;
    }

    if ($('#network #ssid').val().length <= 32)
    {
        $('#network #fg_ssid').removeClass('has-error');
    }
    else
    {
        $('#network #fg_ssid').addClass('has-error');
        WifiSaveDisabled = true;
    }

    if ($('#network #passphrase').val().length <= 64)
    {
        $('#network #fg_password').removeClass('has-error');
    }
    else
    {
        $('#network #fg_password').addClass('has-error');
        WifiSaveDisabled = true;
    }

    if ($('#network #dhcp').prop('checked') === false)
    {
        var iptest = new RegExp(''
            + /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
            + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
            + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
            + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.source
        );

        if (iptest.test($('#network #ip').val()))
        {
            $('#network #fg_ip').removeClass('has-error');
        }
        else
        {
            $('#network #fg_ip').addClass('has-error');
            WifiSaveDisabled = true;
        }

        if (iptest.test($('#network #networkmask').val()))
        {
            $('#network #fg_netmask').removeClass('has-error');
        }
        else
        {
            $('#network #fg_netmask').addClass('has-error');
            WifiSaveDisabled = true;
        }

        if (iptest.test($('#network #gateway').val()))
        {
            $('#network #fg_gateway').removeClass('has-error');
        }
        else
        {
            $('#network #fg_gateway').addClass('has-error');
            WifiSaveDisabled = true;
        }
    }

    $('#network #btn_wifi').prop('disabled', WifiSaveDisabled);
}

function ProcessOutputModeConfiguration()
{
    // console.info("ProcessOutputModeConfiguration: Start");

    // determine the type of output that has been selected and populate the form
    var TypeOfOutputId = parseInt($("#config #device #output option:selected").val(), 10);

    // At the moment we only support a single channel
    var channelId        = 0;
    var channelConfigSet = om_config.om_channels[channelId];
    if (isNaN(TypeOfOutputId))
    {
        // use the value we got from the controller
        TypeOfOutputId = channelConfigSet.om_channel_type;
    }
    var channelConfig    = channelConfigSet[TypeOfOutputId];
    var ChannelTypeName  = channelConfig.type.toLowerCase();

    // clear the array
    selector = [];

    // push the prefix that identifies the object being modified.
    selector.push("#" + ChannelTypeName + " #fg_" + ChannelTypeName);

    // update the fields based on config data
    updateFromJSON(channelConfig);

    // clear the array
    selector = [];

    // console.info("ProcessOutputModeConfiguration: End");

} // ProcessOutputModeConfiguration

function ProcessInputModeConfiguration()
{
    // console.info("ProcessInputModeConfiguration: Start");

    // determine the type of output that has been selected and populate the form
    var TypeOfInputId = parseInt($("#config #device #input option:selected").val(), 10);

    // At the moment we only support a single channel
    var channelId        = 0;
    var channelConfigSet = im_config.im_inputs[channelId];
    if (isNaN(TypeOfInputId))
    {
        // use the value we got from the controller
        TypeOfInputId = channelConfigSet.im_input_type;
    }

    var channelConfig    = channelConfigSet[TypeOfInputId];
    var ChannelTypeName  = channelConfig.type.toLowerCase();

    // clear the array
    selector = [];

    // push the prefix that identifies the object being modified.
    selector.push("#" + ChannelTypeName + " #fg_" + ChannelTypeName);

    // update the fields based on config data
    updateFromJSON(channelConfig);

    // clear the array
    selector = [];

    // console.info("ProcessInputModeConfiguration: Done");

} // ProcessInputModeConfiguration

function ProcessReceivedJsonConfigMessage(JsonConfigData)
{
    // console.info("ProcessReceivedJsonConfigMessage: Start");

    // is this an output config?
    if (JsonConfigData.hasOwnProperty("om_config"))
    {
        // save the config for later use.
        om_config = JsonConfigData.om_config;
        ProcessOutputModeConfiguration();    }

    // is this an input config?
    else if (JsonConfigData.hasOwnProperty("im_config"))
    {
        // save the config for later use.
        im_config = JsonConfigData.im_config;
        ProcessInputModeConfiguration();
    }

    // is this an input config?
    else if (JsonConfigData.hasOwnProperty("device"))
    {
        updateFromJSON(JsonConfigData);
    }

    // is this an input config?
    else if (JsonConfigData.hasOwnProperty("network"))
    {
        updateFromJSON(JsonConfigData);
    }

    // is this an input config?
    else if (JsonConfigData.hasOwnProperty("OK"))
    {
        // console.info("Received Acknowledgement to config set command.")
    }

    else
    {
        console.error("unknown configuration record type has been ignored.")
    }

    // console.info("ProcessReceivedJsonConfigMessage: Done");

} // ProcessReceivedJsonConfigMessage

// Builds jQuery selectors from JSON data and updates the web interface
function updateFromJSON(obj)
{
    for (var k in obj)
    {
        selector.push('#' + k);
        if (typeof obj[k] === 'object' && obj[k] !== null)
        {
            updateFromJSON(obj[k]);
        }
        else
        {
            var jqSelector = selector.join(' ');
            if (typeof obj[k] === 'boolean')
            {
                $(jqSelector).prop('checked', obj[k]);
            }
            else
            {
                $(jqSelector).val(obj[k]);
            }

            // Trigger keyup / change events
            $(jqSelector).trigger('keyup');
            $(jqSelector).trigger('change');
        }
        selector.pop();
    }

    // Update Device ID in footer
    $('#device-id').text($('#config #id').val());
}

function ProcessReceivedOptionDataMessage(JsonOptionList)
{
    // for each field we need to populate
    Object.keys(JsonOptionList).forEach(function (OptionListName)
    {
        var DisplayedChannel  = 0;
        var ArrayOfOptions    = JsonOptionList[OptionListName].list; // value
        var ArrayOfSelections = JsonOptionList[OptionListName].selectedoptionlist;
        var currentSelection  = ArrayOfSelections[DisplayedChannel].selectedoption;
        var jqSelector        = "#" + OptionListName;

        // remove the existing options
        $(jqSelector).empty();

        // for each option in the list
        ArrayOfOptions.forEach(function (listEntry) {
            // add in a new entry
            $(jqSelector).append('<option value="' + listEntry.id + '">' + listEntry.name + '</option>');
        });

        // set the current selector value
        $(jqSelector).val(currentSelection);
    }); // end for each option group

    LoadInputConfigurationForm();
    LoadOutputConfigurationForm();

} // ProcessReceivedOptionDataMessage

// Builds jQuery selectors from JSON data and populates select options
function addOptionDataElementFromJSON()
{
    for (var k in obj)
    {
        selector.push('#' + k);
        if (typeof obj[k] === 'object' && obj[k] !== null)
        {
            addOptionDataElementFromJSON(obj[k]);
        }
        else
        {
            // Get the key and remove the json array index
            var key = selector.pop().substr(1);
            selector.pop();
            var jqSelector = selector.join(' ');
            selector.push('', '');
            $(jqSelector).append('<option value="' + key + '">' + obj[k] + '</option>');
        }
        selector.pop();
    }
}

// Buils JSON config submission for "WiFi" tab - Needs updated
function submitWiFiConfig()
{
    var ip = $('#ip').val().split('.');
    var netmask = $('#netmask').val().split('.');
    var gateway = $('#gateway').val().split('.');

    var json =
    {
        'system':
        {
            'network':
            {
                'ssid': $('#ssid').val(),
                'passphrase': $('#passphrase').val(),
                'hostname': $('#hostname').val(),
                'sta_timeout': $('#sta_timeout').val(),
                'ip': [parseInt(ip[0]), parseInt(ip[1]), parseInt(ip[2]), parseInt(ip[3])],
                'netmask': [parseInt(netmask[0]), parseInt(netmask[1]), parseInt(netmask[2]), parseInt(netmask[3])],
                'gateway': [parseInt(gateway[0]), parseInt(gateway[1]), parseInt(gateway[2]), parseInt(gateway[3])],
                'dhcp': $('#dhcp').prop('checked'),
                'ap_fallback': $('#ap').prop('checked'),
                'ap_timeout': $('#apt').prop('checked')
            }
        }
    };
    wsEnqueue('S1' + JSON.stringify(json));
}

// Build dynamic JSON config submission for "Device" tab
function submitDeviceConfig()
{
    // Build input mode JSON data for submission
    // Build output mode JSON data for submission
    var inputids = [];
    inputids = $('#config #imode *[id]').filter(":input").map(function ()
    {
        return $(this).attr('id');
    }).get();

    var InputChannelId     = 0;
    var InputType          = parseInt($("#config #device #input option:selected").val(), 10);
    var InputConfig        = im_config;
    var InputChannelConfig = InputConfig.im_inputs[InputChannelId][InputType];

    // tell the ESP what type of output it should be using
    InputConfig.im_inputs[InputChannelId].im_input_type = InputType;

    inputids.forEach(function (id)
    {
        var select = '#config #imode #' + id;
        if ($(select).is(':checkbox'))
        {
            InputChannelConfig[id] = $(select).prop('checked');
        }
        else
        {
            InputChannelConfig[id] = $(select).val();
        }
    });

    // Build output mode JSON data for submission
    var outputids = [];
    outputids = $('#config #omode *[id]').filter(":input").map(function ()
    {
        return $(this).attr('id');
    }).get();

    var OutputChannelId     = 0;
    var OutputType          = parseInt($("#config #device #output option:selected").val(),10);
    var OutputConfig        = om_config;
    var OutputChannelConfig = OutputConfig.om_channels[OutputChannelId][OutputType];

    // tell the ESP what type of input it should be using
    OutputConfig.om_channels[OutputChannelId].om_channel_type = OutputType;

    outputids.forEach(function (id)
    {
        var select = '#config #omode #' + id;
        if ($(select).is(':checkbox'))
        {
            OutputChannelConfig[id] = $(select).prop('checked');
        }
        else
        {
            OutputChannelConfig[id] = $(select).val();
        }
    });

    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'device': { 'id': $('#config #device #id').val() } } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'input' : { 'im_config': im_config } } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'output': { 'om_config': om_config } } } }));
}

////////////////////////////////////////////////////
//
//  Websocket stuff
//
////////////////////////////////////////////////////
// On websocket connect
function wsConnect()
{
    if ('WebSocket' in window)
    {
        var target;
        if (!(target = ParseParameter('target')))
        {
            target = document.location.host;
        }

        // target = "192.168.10.99";
        target = "192.168.10.102";

        // Open a new web socket and set the binary type
        ws = new WebSocket('ws://' + target + '/ws');
        ws.binaryType = 'arraybuffer';

        // When connection is opened, get core data.
        // Module data is loaded in module change / load callbacks
        ws.onopen = function ()
        {
            // console.info("ws.onopen");

            $('#wserror').modal('hide');                               // Remove error modal
            $('.wsopt').empty();                                       // Clear out option data built from websockets

            // throw away any old messages
            // console.info("ws.onopen: Flush and Halt");
            wsFlushAndHaltTheOutputQueue();

            // show we are ready to start processing the output queue
            // console.info("ws.onopen: Turn On Sending");
            wsReadyToSend();

            // console.info("ws.onopen: Start Sending");
            wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'device' } })); // Get network config

            // kick start the config screen
            if ($(location).attr("hash") === "#config")
            {
                wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'output' } })); // Get output config
                wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'input' } }));  // Get input config
                wsEnqueue(JSON.stringify({ 'cmd': { 'opt': 'device' } })); // Get device option data
            }

            // console.info("href = " + $(location).attr("hash"));
            if ($(location).attr("hash") === "#admin")
            {
                wsEnqueue('XA');
            }

            RequestStatusUpdate();                                                       // start self filling status loop
        };

        ws.onmessage = function (event)
        {
            // console.info("ws.onmessage: Start");
            if (typeof event.data === "string")
            {
                console.info("ws.onmessage: Received: " + event.data);

                // Process "simple" message format
                if (event.data.startsWith("X"))
                {
                    if (event.data[1] === 'J')
                    {
                        var data = event.data.substr(2);
                        ProcessRecievedJsonStatusMessage(data);
                    }
                    else if (event.data[1] === 'A')
                    {
                        var data = event.data.substr(2);
                        ProcessRecievedJsonAdminMessage(data);
                    }
                }
                else
                {
                    var msg = JSON.parse(event.data);
                    // "GET" message is a response to a get request. Populate the frontend.
                    if (msg.hasOwnProperty("get"))
                    {
                        ProcessReceivedJsonConfigMessage(msg.get);
                    }

                    // "SET" message is a reponse to a set request. Data has been validated and saved, Populate the frontend.
                    if (msg.hasOwnProperty("set"))
                    {
                        ProcessReceivedJsonConfigMessage(msg.set);
                    }

                    // "OPT" message is select option data
                    if (msg.hasOwnProperty("opt"))
                    {
                        ProcessReceivedOptionDataMessage(msg.opt);
                    }
                }
            }
            else
            {
                console.info("Stream Data");

                streamData = new Uint8Array(event.data);
                drawStream(streamData);
                if ($('#diag').is(':visible'))
                {
                    wsEnqueue('V1');
                }
            }

            // show we are ready to send stupp
            // console.info("ws.onmessage: Ask To Send Next Msg");
            wsReadyToSend();

            // console.info("ws.onmessage: Done");
        }; // onmessage

        ws.onclose = function ()
        {
            wsFlushAndHaltTheOutputQueue();
            $('#wserror').modal();
            wsConnect();
        };
    }
    else
    {
        alert('WebSockets is NOT supported by your Browser! You will need to upgrade your browser or downgrade to v2.0 of the ESPixelStick firmware.');
    }
}

// Websocket message queuer
function wsEnqueue(message)
{
    // only send messages if the WS interface is up
    if (ws.readyState !== 1)
    {
        // console.info("WS is down. Discarding msg: " + message);
    }
    else
    {
        wsOutputQueue.push(message);
        wsProcessOutputQueue();

    } // WS is up
} // wsEnqueue

function wsFlushAndHaltTheOutputQueue()
{
    // do we have a send timer running?
    if (null !== wsTimerId)
    {
        // stop the timer
        clearTimeout(wsTimerId);
        wsTimerId = null;
    }

    // show we are ready NOT to send the next message
    wsBusy = true;

    // empty the output queue
    while (wsOutputQueue.length > 0)
    {
        //get the next message from the queue.
        var message = wsOutputQueue.shift();
        // console.info("Discarding msg: " + message);
    }
} // wsFlushAndHaltTheOutputQueue

// Websocket message queuer
function wsProcessOutputQueue()
{
    // console.log('wsProcessOutputQueue');

    // only send messages if the WS interface is up
    if (ws.readyState !== 1)
    {
        // The interface is NOT up. Flush the queue
        // console.log('wsProcessOutputQueue: WS Down. Flush');
        wsFlushAndHaltTheOutputQueue();
    }
    
    //check if we are currently waiting for a response
    else if (wsBusy === true)
    {
        // console.log('wsProcessOutputQueue: Busy');
    } // cant send yet

    else if (wsOutputQueue.length > 0)
    {
        //set the wsBusy flag indicating that we are waiting for a response
        wsBusy = true;

        //get the next message from the queue.
        var OutputMessage = wsOutputQueue.shift();

        // set WaitForResponseTimeMS to clear flag and try next message if response 
        // isn't recieved. 
        var WaitForResponseTimeMS = 20000; // 20 seconds

        // Short WaitForResponseTimeMS for message types that don't generate a response.
        var UseShortDelay = ['T0', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7', 'X6'].indexOf(OutputMessage.substr(0, 2));
        if (UseShortDelay !== -1)
        {
            // warning, setting this value too low can cause a rentrance issue
            WaitForResponseTimeMS = 50;
        }

        // set up a new timer
        wsTimerId = setTimeout(function ()
        {
            // console.info('WS Send Timer expired');

            // Move on to the next message
            wsReadyToSend();

        }, WaitForResponseTimeMS);

        //send it.
        console.log('WS sending ' + OutputMessage);
        ws.send(OutputMessage);

    } // message available to send

} // wsProcessOutputQueue

// Websocket message queuer
function wsReadyToSend()
{
    // is a timer running?
    if (null !== wsTimerId)
    {
        // stop the timer
        clearTimeout(wsTimerId);
        wsTimerId = null;
    }

    // show we are ready to send the next message
    wsBusy = false;

    //send next message
    wsProcessOutputQueue();

} // wsReadyToSend

// Move to diagnostics
function drawStream(streamData)
{
    var cols = parseInt($('#v_columns').val());
    var size = Math.floor((canvas.width - 20) / cols);
    if ($("input[name='viewStyle'][value='RGB']").prop('checked'))
    {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)) * 3);
        for (i = 0; i < maxDisplay; i += 3)
        {
            ctx.fillStyle = 'rgb(' + streamData[i + 0] + ',' + streamData[i + 1] + ',' + streamData[i + 2] + ')';
            var col = (i / 3) % cols;
            var row = Math.floor((i / 3) / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 1, size - 1);
        }
    }
    else
    {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)));
        for (i = 0; i < maxDisplay; i++)
        {
            ctx.fillStyle = 'rgb(' + streamData[i] + ',' + streamData[i] + ',' + streamData[i] + ')';
            var col = (i) % cols;
            var row = Math.floor(i / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 2, size - 2);
        }
    }
    if (streamData.length > maxDisplay)
    {
        ctx.fillStyle = 'rgb(204,0,0)';
        ctx.fillRect(0, canvas.height - 25, canvas.width, 25);
        ctx.fillStyle = 'rgb(255,255,255)';
        ctx.fillText("Increase number of columns to show all data", (canvas.width / 2), canvas.height - 5);
    }
}

// Move to diagnostics
function clearStream()
{
    if (typeof ctx !== 'undefined')
    {
        ctx.clearRect(0, 0, canvas.width, canvas.height);
    }
}

function ProcessRecievedJsonAdminMessage(data)
{
    ParsedJsonAdmin = JSON.parse(data);

    $('#version').text(ParsedJsonAdmin.admin.version);
    $('#built').text(ParsedJsonAdmin.admin.built);
    $('#usedflashsize').text(ParsedJsonAdmin.admin.usedflashsize);
    $('#realflashsize').text(ParsedJsonAdmin.admin.realflashsize);
    $('#flashchipid').text(ParsedJsonAdmin.admin.flashchipid);

} // ProcessRecievedJsonAdminMessage

function ProcessRecievedJsonStatusMessage(data)
{
    ParsedJsonStatus = JSON.parse(data);

    var rssi = ParsedJsonStatus.status.system.rssi;
    var quality = 2 * (rssi + 100);

    if (rssi <= -100)
    {
        quality = 0;
    }
    else if (rssi >= -50)
    {
        quality = 100;
    }

    $('#x_rssi').text     (rssi);
    $('#x_quality').text  (quality);
    $('#x_ssid').text     (ParsedJsonStatus.status.system.ssid);
    $('#x_ip').text       (ParsedJsonStatus.status.system.ip);
    $('#x_hostname').text (ParsedJsonStatus.status.system.hostname);
    $('#x_subnet').text   (ParsedJsonStatus.status.system.subnet);
    $('#x_mac').text      (ParsedJsonStatus.status.system.mac);

    // getHeap(data)
    $('#x_freeheap').text(ParsedJsonStatus.status.system.freeheap);

    // getUptime
    // uptime is reported in milliseconds
    var date = new Date(ParsedJsonStatus.status.system.uptime);
    var str = '';

    //    var hoursPerDay = 24;
    //    var MinutesPerHour = 60;
    //    var secondsPerMinute = 60;
    //    var millisecondsPerSecond = 1000;
    //    var MillisecondsPerDay = millisecondsPerSecond * secondsPerMinute * MinutesPerHour * hoursPerDay; = 86400000

    str += Math.floor(date.getTime() / 86400000) + " days, ";
    str += ("0" + date.getUTCHours()).slice(-2) + ":";
    str += ("0" + date.getUTCMinutes()).slice(-2) + ":";
    str += ("0" + date.getUTCSeconds()).slice(-2);
    $('#x_uptime').text(str);

    // getE131Status(data)
    $('#uni_first').text (ParsedJsonStatus.status.input[0].e131.unifirst);
    $('#uni_last').text  (ParsedJsonStatus.status.input[0].e131.unilast);
    $('#pkts').text      (ParsedJsonStatus.status.input[0].e131.num_packets);
    $('#chanlim').text   (ParsedJsonStatus.status.input[0].e131.unichanlim);
    $('#perr').text      (ParsedJsonStatus.status.input[0].e131.packet_errors);
    $('#clientip').text  (ParsedJsonStatus.status.input[0].e131.last_clientIP);

    // Device Refresh is dynamic
    $('#refresh').text(ParsedJsonStatus.status.output[0].framerefreshrate + " fps");
} // ProcessRecievedJsonAdminMessage

// Show "save" snackbar for 3sec
function snackSave()
{
    var x = document.getElementById('snackbar');
    x.className = 'show';
    setTimeout(function ()
    {
        x.className = x.className.replace('show', '');
    }, 3000);
}

// Needs updated
function setConfig()
{
    // Get config to refresh UI and show result
    wsEnqueue("G1");
    snackSave();
}

// Show reboot modal
function showReboot()
{
    $('#update').modal('hide');
    $('#reboot').modal();
    setTimeout(function ()
    {
        if ($('#dhcp').prop('checked'))
        {
            window.location.assign("/");
        }
        else
        {
            window.location.assign("http://" + $('#ip').val());
        }
    }, 5000);
}

// Queue reboot
function reboot()
{
    showReboot();
    wsEnqueue('X6');
}
