var mode = 'null';
var wsOutputQueue = [];
var wsBusy = false;
var wsOutputQueueTimer = null;
var StatusRequestTimer = null;
var FseqFileListRequestTimer = null;
var ws = null; // Web Socket

// global data
var ParsedJsonStatus = null;
var ParsedJsonConfig = null;
var Output_Config = null; // Output Manager configuration record
var Input_Config = null; // Input Manager configuration record
var Fseq_File_List = null;
var selector = [];
var StatusUpdateRequestTimer = null;
var FileTree = null;
var target = null;
var myDropzone = null;
var SdCardIsInstalled = false;

// Drawing canvas - move to diagnostics
var canvas = document.getElementById("canvas");
var ctx = canvas.getContext("2d");
ctx.font = "20px Arial";
ctx.textAlign = "center";

// Default modal properties
$.fn.modal.Constructor.DEFAULTS.backdrop = 'static';
$.fn.modal.Constructor.DEFAULTS.keyboard = false;

// lets get started
wsConnect();

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

        ProcessWindowChange($($(this))[0].hash);

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

    // DHCP field toggles
    $('#network #dhcp').change(function () {
        if ($(this).is(':checked')) {
            $('.dhcp').addClass('hidden');
        }
        else {
            $('.dhcp').removeClass('hidden');
        }
    });

    $('#DeviceConfigSave').click(function () {
        submitDeviceConfig();
    });

    $('#btn_wifi').click(function () {
        submitWiFiConfig();
    });

    $('#btn_RGB').change(function () {
        clearStream();
    });

    $('#btn_RGBW').change(function () {
        clearStream();
    });

    $('#btn_Channel').change(function () {
        clearStream();
    });

    var finalUrl = "http://" + target + "/upload";
    // console.log(finalUrl);
    const uploader = new Dropzone('#filemanagementupload', 
    {
        url: finalUrl,
        paramName: 'file',
        maxFilesize: 1000, // MB
        maxFiles: 1,
        parallelUploads: 1,
        clickable: true,
        uploadMultiple: false, 
        createImageThumbnails: false,
        dictDefaultMessage: 'Drag an image here to upload, or click to select one',
        acceptedFiles: '.fseq',
        timeout: 999999, /*milliseconds*/
        init: function ()
        {
            this.on('success', function (file, resp)
            {
                // console.log("Success");
                // console.log(file);
                // console.log(resp);
                Dropzone.forElement('#filemanagementupload').removeAllFiles(true)
                RequestListOfFiles();
            });

            this.on('addedfile', function (file, resp)
            {
                // console.log("addedfile");
                // console.log(file);
                // console.log(resp);
            });

        },

        accept: function (file, done)
        {
            // console.log("accept");
            // console.log(file);
            return done(); // triggers a send
        }
    });
    $("#filemanagementupload").addClass("dropzone");

    $('#FileDeleteButton').click(function ()
    {
        RequestFileDeletion();
    });

    // Autoload tab based on URL hash
    var hash = window.location.hash;
    hash && $('ul.navbar-nav li a[href="' + hash + '"]').click();

    RequestListOfFiles();

    // start updating stats
    RequestStatusUpdate();
});

function ProcessWindowChange(NextWindow) {

    if (NextWindow === "#diag") {
        wsEnqueue('V1');
    }

    else if (NextWindow === "#admin") {
        wsEnqueue('XA');
    }

    else if ((NextWindow === "#wifi") || (NextWindow === "#home")) {
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'device' } })); // Get general config
    }

    // kick start the live stream
    else if (NextWindow === "#config") {
        RequestListOfFiles();
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'output' } })); // Get output config
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'input' } }));  // Get input config
        wsEnqueue(JSON.stringify({ 'cmd': { 'opt': 'input' } })); // Get device option data
        wsEnqueue(JSON.stringify({ 'cmd': { 'opt': 'output' } })); // Get device option data
    }

    RequestListOfFiles();

} // ProcessWindowChange

function RequestStatusUpdate()
{
    // is the timer running?
    if (null === StatusRequestTimer)
    {
        // timer runs forever
        StatusRequestTimer = setTimeout(function ()
        {
            clearTimeout(StatusRequestTimer);
            StatusRequestTimer = null;

            RequestStatusUpdate();

        }, 1000);
    } // end timer was not running

    if ($('#home').is(':visible'))
    {
        // ask for a status update from the server
        wsEnqueue('XJ');
    } // end home (aka status) is visible

} // RequestStatusUpdate

function RequestListOfFiles()
{
    // is the timer running?
    if (null === FseqFileListRequestTimer)
    {
        // timer runs until we get a response
        FseqFileListRequestTimer = setTimeout(function ()
        {
            clearTimeout(FseqFileListRequestTimer);
            FseqFileListRequestTimer = null;

            RequestListOfFiles();

        }, 1000);
    } // end timer was not running

    // ask for a file list from the server
    wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'files' } })); // Get File List

} // RequestListOfFseqFiles

function ProcessGetFileResponse(JsonConfigData)
{
    // console.info("ProcessGetFileResponse");


    SdCardIsInstalled = JsonConfigData.SdCardPresent;

    $("#li-filemanagement").removeClass("hidden");
    if (false === SdCardIsInstalled)
    {
        $("#li-filemanagement").addClass("hidden");
    }

    Fseq_File_List = JsonConfigData;

    clearTimeout(FseqFileListRequestTimer);
    FseqFileListRequestTimer = null;

    var TreeRoot = new TreeNode("List Of Files");
    JsonConfigData.files.forEach(function (file)
    {
        var NewFileNode = new TreeNode(file.name);
        TreeRoot.addChild(NewFileNode);
    });

    FileTree = new TreeView(TreeRoot, "#FileTree");

} // ProcessGetFileResponse

function RequestFileDeletion()
{
    var ListOfSelectedFiles = FileTree.getSelectedNodes(); 
    if (0 < ListOfSelectedFiles.length) {

        var files = [];

        ListOfSelectedFiles.forEach(function (file) {
            var FileEntry = {};
            FileEntry["name"] = file.toString();
            files.push(FileEntry);

            // console.info(file.toString());
        });

        wsEnqueue(JSON.stringify({ 'cmd': { 'delete': { 'files' : files }}}));
    }

} // RequestFileDeletion

function ParseParameter(name)
{
    return (location.search.split(name + '=')[1] || '').split('&')[0];
}

function ProcessModeConfigurationDatafppremote(channelConfig)
{
    var jqSelector = "#fseqfilename";

    // remove the existing options
    $(jqSelector).empty();

    $(jqSelector).append('<option value=>Play Remote Sequence</option>');

    // for each file in the list
    Fseq_File_List.files.forEach(function (listEntry) {
        // add in a new entry
        $(jqSelector).append('<option value="' + listEntry.name + '">' + listEntry.name + '</option>');
    });

    // set the current selector value
    $(jqSelector).val(channelConfig.fseqfilename);


} // ProcessModeConfigurationDatafppremote

function ProcessModeConfigurationDataEffects(channelConfig)
{
    var jqSelector = "#currenteffect";

    // remove the existing options
    $(jqSelector).empty();

    // for each option in the list
    channelConfig.effects.forEach(function (listEntry) {
        // add in a new entry
        $(jqSelector).append('<option value="' + listEntry.name + '">' + listEntry.name + '</option>');
    });

    // set the current selector value
    $(jqSelector).val(channelConfig.currenteffect);

} // ProcessModeConfigurationDataEffects

function ProcessModeConfigurationDataRelay(RelayConfig)
{
    // console.log("relaychannelconfigurationtable.rows.length = " + $('#relaychannelconfigurationtable tr').length);

    $('#updateinterval').val(parseInt(RelayConfig.updateinterval,10));

    var ChannelConfigs = RelayConfig.channels;

    while (1 < $('#relaychannelconfigurationtable tr').length) {
        // console.log("Deleting $('#relaychannelconfigurationtable tr').length " + $('#relaychannelconfigurationtable tr').length);
        RelayTableRef.last().remove();
        // console.log("After Delete: $('#relaychannelconfigurationtable tr').length " + $('#relaychannelconfigurationtable tr').length);
    }

    // add as many rows as we need
    for (CurrentRowId = 1; CurrentRowId <= ChannelConfigs.length; CurrentRowId++)
    {
        console.log("CurrentRowId = " + CurrentRowId);
        var ChanIdPattern     = '<td id="chanId_'                            + (CurrentRowId) + '">a</td>';
        var EnabledPattern    = '<td><input type="checkbox" id="Enabled_'    + (CurrentRowId) + '"></td>';
        var InvertedPattern   = '<td><input type="checkbox" id="Inverted_'   + (CurrentRowId) + '"></td>';
        var gpioPattern       = '<td><input type="number"   id="gpioId_'     + (CurrentRowId) + '"step="1" min="0" max="24"  value="30"  class="form-control is-valid"></td>';
        var threshholdPattern = '<td><input type="number"   id="threshhold_' + (CurrentRowId) + '"step="1" min="0" max="255" value="300" class="form-control is-valid"></td>';

        var rowPattern = '<tr>' + ChanIdPattern + EnabledPattern + InvertedPattern + gpioPattern + threshholdPattern + '</tr>';
        $('#relaychannelconfigurationtable tr:last').after(rowPattern);

        $('#chanId_'     + CurrentRowId).attr('style', $('#chanId_hr').attr('style'));
        $('#Enabled_'    + CurrentRowId).attr('style', $('#Enabled_hr').attr('style'));
        $('#Inverted_'   + CurrentRowId).attr('style', $('#Inverted_hr').attr('style'));
        $('#gpioId_'     + CurrentRowId).attr('style', $('#gpioId_hr').attr('style'));
        $('#threshhold_' + CurrentRowId).attr('style', $('#threshhold_hr').attr('style'));
    }

    $.each(ChannelConfigs, function (i, CurrentChannelConfig)
    {
        console.log("Current Channel Id = " + CurrentChannelConfig.id);
        var currentChannelRowId = CurrentChannelConfig.id + 1;
        $('#chanId_'     + (currentChannelRowId)).html(currentChannelRowId);
        $('#Enabled_'    + (currentChannelRowId)).prop("checked", CurrentChannelConfig.enabled);
        $('#Inverted_'   + (currentChannelRowId)).prop("checked", CurrentChannelConfig.invertoutput);
        $('#gpioId_'     + (currentChannelRowId)).val(parseInt(CurrentChannelConfig.gpioid,10));
        $('#threshhold_' + (currentChannelRowId)).val(parseInt(CurrentChannelConfig.onofftriggerlevel, 10));
    });

} // ProcessModeConfigurationDataRelay

function ProcessModeConfigurationData(channelId, ChannelTypeName, JsonConfig )
{
    // console.info("ProcessModeConfigurationData: Start");

    // determine the type of output that has been selected and populate the form
    var TypeOfChannelId = parseInt($('#' + ChannelTypeName +  channelId + " option:selected").val(), 10);
    var channelConfigSet = JsonConfig.channels[channelId];

    if (isNaN(TypeOfChannelId))
    {
        // use the value we got from the controller
        TypeOfChannelId = channelConfigSet.type;
    }
    var channelConfig = channelConfigSet[TypeOfChannelId];
    ChannelTypeName = channelConfig.type.toLowerCase();
    ChannelTypeName = ChannelTypeName.replace(".", "_");
    ChannelTypeName = ChannelTypeName.replace(" ", "_");
    // console.info("ChannelTypeName: " + ChannelTypeName);

    // clear the array
    selector = [];

    // push the prefix that identifies the object being modified.
    selector.push(" #" + ChannelTypeName);

    // update the fields based on config data
    updateFromJSON(channelConfig);

    // clear the array
    selector = [];

    if (("fppremote" === ChannelTypeName) && (null !== Fseq_File_List))
    {
        ProcessModeConfigurationDatafppremote(channelConfig);
    }

    if ("effects" === ChannelTypeName)
    {
        ProcessModeConfigurationDataEffects(channelConfig);
    }

    if ("relay" === ChannelTypeName)
    {
        console.info("ProcessModeConfigurationData: relay");
        ProcessModeConfigurationDataRelay(channelConfig);
    }
    // console.info("ProcessModeConfigurationData: End");

} // ProcessModeConfigurationData

function ProcessReceivedJsonConfigMessage(JsonConfigData)
{
    // console.info("ProcessReceivedJsonConfigMessage: Start");

    // is this an output config?
    if (JsonConfigData.hasOwnProperty("output_config"))
    {
        // save the config for later use.
        Output_Config = JsonConfigData.output_config;
    }

    // is this an input config?
    else if (JsonConfigData.hasOwnProperty("input_config"))
    {
        // save the config for later use.
        Input_Config = JsonConfigData.input_config;
    }

    // is this a device config?
    else if (JsonConfigData.hasOwnProperty("device"))
    {
        updateFromJSON(JsonConfigData);
    }

    // is this a network config?
    else if (JsonConfigData.hasOwnProperty("network"))
    {
        updateFromJSON(JsonConfigData);
    }

    // is this a file list?
    else if (JsonConfigData.hasOwnProperty("files"))
    {
        ProcessGetFileResponse(JsonConfigData);
    }

    // is this an ACK response?
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

function GenerateInputOutputControlName(OptionListName, DisplayedChannelId)
{
    var NewName;

    if (0 === DisplayedChannelId)
    {
        NewName = "Primary " + OptionListName + " ";
    }

    if (1 === DisplayedChannelId)
    {
        NewName = "Secondary " + OptionListName + " ";
    }

    if (3 === DisplayedChannelId)
    {
        NewName = "Tertiary " + OptionListName + " ";
    }

    return NewName;
} // GenerateInputOutputControlName

function ProcessReceivedOptionDataMessage(JsonOptionList)
{
    // console.info("ProcessReceivedOptionDataMessage");

    // for each field we need to populate (input vs output)
    Object.keys(JsonOptionList).forEach(function (OptionListName)
    {
        // OptionListName is 'input' or 'output'
        var ArrayOfOptions    = JsonOptionList[OptionListName].list; // value
        var ArrayOfSelections = JsonOptionList[OptionListName].selectedoptionlist;

        // for each option channel:
        $(ArrayOfSelections).each(function (DisplayedChannelId, currentSelection)
        {
            // does the selection box we need already exist?
            if (! $('#' + OptionListName + 'mode' + DisplayedChannelId).length)
            {
                // create the selection box
                $('#fg_' + OptionListName).append('<label class="control-label col-sm-2" for="' + OptionListName + DisplayedChannelId + '">' + GenerateInputOutputControlName(OptionListName, DisplayedChannelId) + ' Mode:</label>');
                $('#fg_' + OptionListName).append('<div class="col-sm-2"><select class="form-control wsopt" id="' + OptionListName + DisplayedChannelId + '"></select></div>');
                $('#fg_' + OptionListName + '_mode').append('<fieldset id="' + OptionListName + 'mode' + DisplayedChannelId + '"></fieldset>');
            }

            // define the input select list action
            $('#' + OptionListName + DisplayedChannelId).change(function ()
            {
                if ($(this).val())
                {
                    var HtmlLoadFileName = $('#' + OptionListName + DisplayedChannelId + ' option:selected').text().toLowerCase();
                    // console.info("HtmlLoadName: " + HtmlLoadName);
                    HtmlLoadFileName = HtmlLoadFileName.replace(".", "_");
                    HtmlLoadFileName = HtmlLoadFileName.replace(" ", "_");
                    HtmlLoadFileName = HtmlLoadFileName + ".html";
                    // console.info("Adjusted HtmlLoadName: " + HtmlLoadName);

                    // try to load the field definition file for this channel type
                    $('#' + OptionListName + 'mode' + DisplayedChannelId).load(HtmlLoadFileName, function ()
                    {
                        if ("input" === OptionListName)
                        {
                            ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Input_Config);
                        }
                        else if ("output" === OptionListName)
                        {
                            ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Output_Config);
                        }
                    });
                }
            });

            var jqSelector = "#" + OptionListName + DisplayedChannelId;

            // remove the existing options
            $(jqSelector).empty();

            // for each option in the list
            ArrayOfOptions.forEach(function (listEntry)
            {
                // add in a new entry
                $(jqSelector).append('<option value="' + listEntry.id + '">' + listEntry.name + '</option>');
            });

            // set the current selector value
            $(jqSelector).val(ArrayOfSelections[DisplayedChannelId].selectedoption);

            // clear the footer
            $('#refresh').html('0 ms / 0 fps');

            var fileName = $('#' + OptionListName + DisplayedChannelId + ' option:selected').text().toLowerCase();
            fileName = fileName.replace(" ", "_").replace(".", "_");
            // console.info("fileName: " + fileName);

            // try to load the field definition file for this channel type
            $('#' + OptionListName + 'mode' + DisplayedChannelId).load(fileName + ".html", function ()
            {
                if ("input" === OptionListName)
                {
                    ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Input_Config);
                }
                else if ("output" === OptionListName)
                {
                    ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Output_Config);
                }
            });

        }); // end for each channel
    }); // end for each option group

} // ProcessReceivedOptionDataMessage

// Builds JSON config submission for "WiFi" tab
function submitWiFiConfig()
{
    var json =
    {
        'cmd':
        {
            'set':
            {
                'network':
                {
                    'ssid': $('#ssid').val(),
                    'passphrase': $('#passphrase').val(),
                    'hostname': $('#hostname').val(),
                    'sta_timeout': $('#sta_timeout').val(),
                    'ip': $('#ip').val(),
                    'netmask': $('#netmask').val(),
                    'gateway': $('#gateway').val(),
                    'dhcp': $('#dhcp').prop('checked'),
                    'ap_fallback': $('#ap').prop('checked'),
                    'ap_timeout': $('#apt').prop('checked')
                }
            }
        }
    };

    wsEnqueue(JSON.stringify(json));

} // submitWiFiConfig

function ExtractDeviceConfigFromHtmlPage(JsonConfig, SectionName)
{
    // for each option channel:
    jQuery.each(JsonConfig, function (DisplayedChannelId, CurrentChannelConfigurationData)
    {
        var elementids = [];
        var modeControlName = '#' + SectionName + 'mode' + DisplayedChannelId;
        elementids = $(modeControlName + ' *[id]').filter(":input").map(function ()
        {
            return $(this).attr('id');
        }).get();

        var ChannelType = parseInt($("#" + SectionName + DisplayedChannelId + " option:selected").val(), 10);
        var ChannelConfig = CurrentChannelConfigurationData[ChannelType];

        // tell the ESP what type of channel it should be using
        CurrentChannelConfigurationData.type = ChannelType;

        elementids.forEach(function (elementid)
        {
            var SelectedElement = modeControlName + ' #' + elementid;
            if ($(SelectedElement).is(':checkbox')) {
                ChannelConfig[elementid] = $(SelectedElement).prop('checked');
            }
            else
            {
                ChannelConfig[elementid] = $(SelectedElement).val();
            }
        });

        if ((ChannelConfig.type === "Relay") && ($("#relaychannelconfigurationtable").length))
        {
            $.each(ChannelConfig.channels, function (i, CurrentChannelConfig)
            {
                console.log("Current Channel Id = " + CurrentChannelConfig.id);
                var currentChannelRowId = CurrentChannelConfig.id + 1;
                CurrentChannelConfig.enabled           = $('#Enabled_'    + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.invertoutput      = $('#Inverted_'   + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.gpioid            = $('#gpioId_'     + (currentChannelRowId)).val();
                CurrentChannelConfig.onofftriggerlevel = $('#threshhold_' + (currentChannelRowId)).val();
            });
        }
    }); // end for each channel
} // ExtractDeviceConfigFromHtmlPage

// Build dynamic JSON config submission for "Device" tab
function submitDeviceConfig()
{
    ExtractDeviceConfigFromHtmlPage(Input_Config.channels,  "input");
    ExtractDeviceConfigFromHtmlPage(Output_Config.channels, "output");

    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'device': { 'id': $('#config #device #id').val() } } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'input': { 'input_config': Input_Config } } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'output': { 'output_config': Output_Config } } } }));

    return;

} // submitDeviceConfig

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
        if (!(target = ParseParameter('target')))
        {
            target = document.location.host;
        }

        // target = "192.168.10.155";
        // target = "192.168.10.162";

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

            ProcessWindowChange($(location).attr("hash"));

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
                // console.info("Stream Data");

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
    if (null !== wsOutputQueueTimer)
    {
        // stop the timer
        clearTimeout(wsOutputQueueTimer);
        wsOutputQueueTimer = null;
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
        wsOutputQueueTimer = setTimeout(function ()
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
    if (null !== wsOutputQueueTimer)
    {
        // stop the timer
        clearTimeout(wsOutputQueueTimer);
        wsOutputQueueTimer = null;
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
    else if ($("input[name='viewStyle'][value='RGBW']").prop('checked')) {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)) * 4);
        for (i = 0; i < maxDisplay; i += 4) {
            ctx.fillStyle = 'rgb(' + streamData[i + 0] + ',' + streamData[i + 1] + ',' + streamData[i + 2] + ')';
            var col = (i / 4) % cols;
            var row = Math.floor((i / 4) / cols);
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
    var Admin = ParsedJsonAdmin.admin;

    $('#version').text(Admin.version);
    $('#built').text(Admin.built);
    $('#usedflashsize').text(Admin.usedflashsize);
    $('#realflashsize').text(Admin.realflashsize);
    $('#flashchipid').text(Admin.flashchipid);

} // ProcessRecievedJsonAdminMessage

//ProcessRecievedJsonStatusMessage
function ProcessRecievedJsonStatusMessage(data)
{
    JsonStat = JSON.parse(data);
    var Status = JsonStat.status;
    var System = Status.system;
    var rssi = System.rssi;
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
    $('#x_ssid').text     (System.ssid);
    $('#x_ip').text       (System.ip);
    $('#x_hostname').text (System.hostname);
    $('#x_subnet').text   (System.subnet);
    $('#x_mac').text      (System.mac);

    // getHeap(data)
    $('#x_freeheap').text(System.freeheap);

    // getUptime
    // uptime is reported in milliseconds
    var date = new Date(System.uptime);
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
    var InputStatus = Status.input[0];
    if (Status.input[0].hasOwnProperty('e131'))
    {
        $('#uni_first').text(InputStatus.e131.unifirst);
        $('#uni_last').text (InputStatus.e131.unilast);
        $('#pkts').text     (InputStatus.e131.num_packets);
        $('#chanlim').text  (InputStatus.e131.unichanlim);
        $('#perr').text     (InputStatus.e131.packet_errors);
        $('#clientip').text (InputStatus.e131.last_clientIP);
    }

    if (Status.input[0].hasOwnProperty('ddp'))
    {
        $('#ddppacketsreceived').text(InputStatus.ddp.packetsreceived);
        $('#ddpbytesreceived').text(InputStatus.ddp.bytesreceived);
        $('#ddperrors').text(InputStatus.ddp.errors);
    }

    // Device Refresh is dynamic
    $('#refresh').text(Status.output[0].framerefreshrate + " fps");
} // ProcessRecievedJsonStatusMessage

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

