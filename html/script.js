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
var AdminInfo = null;
var Output_Config = null; // Output Manager configuration record
var Input_Config = null; // Input Manager configuration record
var Device_Config = null;
var Network_Config = null;
var Fseq_File_List = null;
var selector = [];
var StatusUpdateRequestTimer = null;
var target = null;
var myDropzone = null;
var SdCardIsInstalled = false;
var FseqFileTransferStartTime = new Date();

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
            var file = _('efu').files[0];
            var formdata = new FormData();
            formdata.append("file", file);
            var FileXfer = new XMLHttpRequest();

            FileXfer.upload.addEventListener("progress", progressHandler, false);
            FileXfer.addEventListener("load", completeHandler, false);
            FileXfer.addEventListener("error", errorHandler, false);
            FileXfer.addEventListener("abort", abortHandler, false);
            FileXfer.open("POST", "http://" + target + "/updatefw");
            FileXfer.send(formdata);
            $("#EfuProgressBar").removeClass("hidden");

            function _(el) {
                return document.getElementById(el);
            }
            function progressHandler(event) {
                var percent = (event.loaded / event.total) * 100;
                _("EfuProgressBar").value = Math.round(percent);
            }

            function completeHandler(event) {
                // _("status").innerHTML = event.target.responseText;
                _("EfuProgressBar").value = 0; //will clear progress bar after successful upload
                showReboot();
            }

            function errorHandler(event) {
                console.error("Transfer Error");
                // _("status").innerHTML = "Upload Failed";
            }

            function abortHandler(event) {
                console.error("Transfer Abort");
                // _("status").innerHTML = "Upload Aborted";
            }
        });
    });

    // DHCP field toggles
    $('#network #dhcp').change(function () {
        if ($(this).is(':checked')) {
            $('.dhcp').removeClass('hidden');
            $('.dhcp').addClass('hidden');
        }
        else {
            $('.dhcp').removeClass('hidden');
        }
        $('#btn_wifi').prop("disabled", ValidateConfigFields($("#network input")));
    });

    $('#network').on("input", (function () {
        $('#btn_wifi').prop("disabled", ValidateConfigFields($("#network input")));
    }));

    $('#config').on("input", (function () {
        $('#DeviceConfigSave').prop("disabled", ValidateConfigFields($('#config input')));
    }));

    $('#DeviceConfigSave').click(function ()
    {
        submitDeviceConfig();
    });

    $('#btn_wifi').click(function () {
        submitWiFiConfig();
    });

    $('#viewStyle').change(function () {
        clearStream();
    });

    $('#v_columns').on('input', function () {
        clearStream();
    });

    $('#backupconfig').click(function ()
    {
        ExtractNetworkConfigFromHtmlPage();
        ExtractChannelConfigFromHtmlPage(Input_Config.channels, "input");
        ExtractChannelConfigFromHtmlPage(Output_Config.channels, "output");
        Device_Config.id = $('#config #device #id').val();

        var TotalConfig = JSON.stringify({ 'device': Device_Config, 'network': Network_Config, 'input': Input_Config, 'output': Output_Config });

        var blob = new Blob([TotalConfig], { type: "text/json;charset=utf-8" });
        var FileName = Device_Config.id.replace(".", "-").replace(" ", "-").replace(",", "-") + "-" + AdminInfo.flashchipid;
        saveAs(blob, FileName + ".json");
    });

    $('#restoreconfig').change(function ()
    {
        if (this.files.length !== 0)
        {
            const reader = new FileReader();
            reader.onload = function fileReadCompleted()
            {
                // when the reader is done, the content is in reader.result.
                ProcessLocalConfig(reader.result);
            };
            reader.readAsText(this.files[0]);
        }
    });

    $('#adminReboot').click(function () {
        reboot();
    });

    $('#adminFactoryReset').click(function () {
        factoryReset();
    });

    $('#AdvancedOptions').change(function () {
        UpdateAdvancedOptionsMode();
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
        acceptedFiles: '.fseq,.pl',
        timeout: 99999999, /*milliseconds*/
        init: function ()
        {
            this.on('success', function (file, resp) {
                // console.log("Success");
                // console.log(file);
                // console.log(resp);
                Dropzone.forElement('#filemanagementupload').removeAllFiles(true)
                RequestListOfFiles();
            });

            this.on('complete', function (file, resp) {
                // console.log("complete");
                // console.log(file);
                // console.log(resp);
                $('#fseqprogress_fg').addClass("hidden");

                var DeltaTime = (new Date().getTime() - FseqFileTransferStartTime.getTime()) / 1000;
                var rate = Math.floor((file.size / DeltaTime) / 1000);
                console.info("Final Transfer Rate: " + rate + "KBps");
            });

            this.on('addedfile', function (file, resp)
            {
                // console.log("addedfile");
                // console.log(file);
                // console.log(resp);
                FseqFileTransferStartTime = new Date();
            });

            this.on('uploadprogress', function (file, percentProgress, bytesSent) {
                // console.log("percentProgress: " + percentProgress);
                // console.log("bytesSent: " + bytesSent);
                $('#fseqprogress_fg').removeClass("hidden");
                $('#fseqprogressbytes').html(bytesSent);

                var now = new Date().getTime();
                var DeltaTime = (now - FseqFileTransferStartTime.getTime()) / 1000;
                var rate = Math.floor((bytesSent / DeltaTime)/1000);
                $('#fseqprogressrate').html(rate + "KBps");
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

    // start updating stats
    RequestStatusUpdate();
});

function ProcessLocalConfig(data)
{
    // console.info(data);
    var ParsedLocalConfig = JSON.parse(data);

    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'device' : ParsedLocalConfig.device, 'network': ParsedLocalConfig.network } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'input'  : { 'input_config' : ParsedLocalConfig.input  } } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'output' : { 'output_config': ParsedLocalConfig.output } } } }));

} // ProcessLocalConfig

function UpdateAdvancedOptionsMode()
{
    console.info("UpdateAdvancedOptionsMode");

    var am = $('#AdvancedOptions');
    var AdvancedModeState = am.prop("checked");

    $(".AdvancedMode").each(function ()
    {
        if (true === AdvancedModeState)
        {
            $(this).removeClass("hidden");
        }
        else
        {
            $(this).addClass("hidden");
        }
    });

} // UpdateAdvancedOptionsMode

function ProcessWindowChange(NextWindow) {

    if (NextWindow === "#diag") {
        wsEnqueue('V1');
    }

    else if (NextWindow === "#admin") {
        wsEnqueue('XA');
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'device' } })); // Get general config
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'output' } })); // Get output config
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'input' } }));  // Get input config
    }

    else if ((NextWindow === "#wifi") || (NextWindow === "#home")) {
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'device' } })); // Get general config
    }

    else if (NextWindow === "#config") {
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'output' } })); // Get output config
        wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'input' } }));  // Get input config
        RequestListOfFiles();
    }

    else if (NextWindow === "#filemanagement") {
        RequestListOfFiles();
    }

    UpdateAdvancedOptionsMode();

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

} // RequestListOfFiles

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

    // console.info("$('#FileManagementTable > tr').length " + $('#FileManagementTable > tr').length);

    while (1 < $('#FileManagementTable > tr').length)
    {
        // console.info("Deleting $('#FileManagementTable tr').length " + $('#FileManagementTable tr').length);
        $('#FileManagementTable tr').last().remove();
        // console.log("After Delete: $('#FileManagementTable tr').length " + $('#FileManagementTable tr').length);
    }

    var CurrentRowId = 0;
    JsonConfigData.files.forEach(function (file)
    {
        var SelectedPattern = '<td><input  type="checkbox" id="FileSelected_' + (CurrentRowId) + '"></td>';
        var NamePattern     = '<td><output type="text"     id="FileName_'     + (CurrentRowId) + '"></td>';
        var DatePattern     = '<td><output type="text"     id="FileDate_'     + (CurrentRowId) + '"></td>';
        var SizePattern     = '<td><output type="text"     id="FileSize_'     + (CurrentRowId) + '"></td>';

        var rowPattern = '<tr>' + SelectedPattern + NamePattern + DatePattern + SizePattern + '</tr>';
        $('#FileManagementTable tr:last').after(rowPattern);

        $('#FileName_' + (CurrentRowId)).val(file.name);
        $('#FileDate_' + (CurrentRowId)).val(new Date(file.date * 1000).toISOString());
        $('#FileSize_' + (CurrentRowId)).val(file.length);

        CurrentRowId++;
    });
} // ProcessGetFileResponse

function RequestFileDeletion()
{
    var files = [];

    $('#FileManagementTable > tr').each(function (CurRowId)
    {
        if (true === $('#FileSelected_' + CurRowId).prop("checked"))
        {
            var FileEntry = {};
            FileEntry["name"] = $('#FileName_' + CurRowId).val().toString();
            files.push(FileEntry);
        }
    });

    wsEnqueue(JSON.stringify({ 'cmd': { 'delete': { 'files': files } } }));
    RequestListOfFiles();

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

    $(jqSelector).append('<option value="...">Play Remote Sequence</option>');

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

    var ChannelConfigs = RelayConfig.channels;

    while (1 < $('#relaychannelconfigurationtable tr').length) {
        // console.log("Deleting $('#relaychannelconfigurationtable tr').length " + $('#relaychannelconfigurationtable tr').length);
        RelayTableRef.last().remove();
        // console.log("After Delete: $('#relaychannelconfigurationtable tr').length " + $('#relaychannelconfigurationtable tr').length);
    }

    // add as many rows as we need
    for (var CurrentRowId = 1; CurrentRowId <= ChannelConfigs.length; CurrentRowId++)
    {
        // console.log("CurrentRowId = " + CurrentRowId);
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
        // console.log("Current Channel Id = " + CurrentChannelConfig.id);
        var currentChannelRowId = CurrentChannelConfig.id + 1;
        $('#chanId_'     + (currentChannelRowId)).html(currentChannelRowId);
        $('#Enabled_'    + (currentChannelRowId)).prop("checked", CurrentChannelConfig.en);
        $('#Inverted_'   + (currentChannelRowId)).prop("checked", CurrentChannelConfig.inv);
        $('#gpioId_'     + (currentChannelRowId)).val(CurrentChannelConfig.gid);
        $('#threshhold_' + (currentChannelRowId)).val(CurrentChannelConfig.trig);
    });

} // ProcessModeConfigurationDataRelay

function ProcessModeConfigurationDataServoPCA9685(ServoConfig)
{
    // console.log("Servochannelconfigurationtable.rows.length = " + $('#servo_pca9685channelconfigurationtable tr').length);

    var ChannelConfigs = ServoConfig.channels;

    while (1 < $('#servo_pca9685channelconfigurationtable tr').length) {
        // console.log("Deleting $('#servo_pca9685channelconfigurationtable tr').length " + $('#servo_pca9685channelconfigurationtable tr').length);
        ServoTableRef.last().remove();
        // console.log("After Delete: $('#servo_pca9685channelconfigurationtable tr').length " + $('#servo_pca9685channelconfigurationtable tr').length);
    }

    // add as many rows as we need
    for (var CurrentRowId = 1; CurrentRowId <= ChannelConfigs.length; CurrentRowId++) {
        // console.log("CurrentRowId = " + CurrentRowId);
        var ChanIdPattern   = '<td                        id="ServoChanId_'                  + (CurrentRowId) + '">a</td>';
        var EnabledPattern  = '<td><input type="checkbox" id="ServoEnabled_'                 + (CurrentRowId) + '"></td>';
        var MinLevelPattern = '<td><input type="number"   id="ServoMinLevel_'                + (CurrentRowId) + '"step="1" min="10" max="4095"  value="0"  class="form-control is-valid"></td>';
        var MaxLevelPattern = '<td><input type="number"   id="ServoMaxLevel_'                + (CurrentRowId) + '"step="1" min="10" max="4095"  value="0"  class="form-control is-valid"></td>';
        var DataType        = '<td><select class="form-control is-valid" id="ServoDataType_' + (CurrentRowId) + '" title="Effect to generate"></select></td>';

        var rowPattern = '<tr>' + ChanIdPattern + EnabledPattern + MinLevelPattern + MaxLevelPattern + DataType + '</tr>';
        $('#servo_pca9685channelconfigurationtable tr:last').after(rowPattern);

        $('#ServoChanId_'   + CurrentRowId).attr('style', $('#ServoChanId_hr').attr('style'));
        $('#ServoEnabled_'  + CurrentRowId).attr('style', $('#ServoEnabled_hr').attr('style'));
        $('#ServoMinLevel_' + CurrentRowId).attr('style', $('#ServoMinLevel_hr').attr('style'));
        $('#ServoMaxLevel_' + CurrentRowId).attr('style', $('#ServoMaxLevel_hr').attr('style'));
        $('#ServoDataType_' + CurrentRowId).attr('style', $('#ServoDataType_hr').attr('style'));
    }

    $.each(ChannelConfigs, function (i, CurrentChannelConfig) {
        // console.log("Current Channel Id = " + CurrentChannelConfig.id);
        var currentChannelRowId = CurrentChannelConfig.id + 1;
        $('#ServoChanId_'   + (currentChannelRowId)).html(currentChannelRowId);
        $('#ServoEnabled_'  + (currentChannelRowId)).prop("checked", CurrentChannelConfig.en);
        $('#ServoMinLevel_' + (currentChannelRowId)).val(CurrentChannelConfig.Min);
        $('#ServoMaxLevel_' + (currentChannelRowId)).val(CurrentChannelConfig.Max);

        var jqSelector = "#ServoDataType_" + (currentChannelRowId);

        // remove the existing options
        $(jqSelector).empty();
        $(jqSelector).append('<option value=0> 8 Bit Absolute</option>');
        $(jqSelector).append('<option value=1> 8 Bit Absolute Reversed</option>');
        $(jqSelector).append('<option value=2> 8 Bit Scaled</option>');
        $(jqSelector).append('<option value=3> 8 Bit Scaled - Reversed</option>');
        $(jqSelector).append('<option value=4>16 Bit Absolute</option>');
        $(jqSelector).append('<option value=5>16 Bit Absolute - Reversed</option>');
        $(jqSelector).append('<option value=6>16 Bit Scaled</option>');
        $(jqSelector).append('<option value=7>16 Bit Scaled - Reversed</option>');

        // set the current selector value
        $(jqSelector).val((CurrentChannelConfig.rev << 0) +
                          (CurrentChannelConfig.sca << 1) +
                          (CurrentChannelConfig.b16 << 2) );
    });

} // ProcessModeConfigurationDataServoPCA9685

function ProcessInputConfig()
{
    $("#ecb_enable").prop("checked", Input_Config.ecb.enabled);
    $("#ecb_gpioid").val(Input_Config.ecb.id);
    $("#ecb_polarity").val(Input_Config.ecb.polarity);

} // ProcessInputConfig

function ProcessModeConfigurationData(channelId, ChannelType, JsonConfig )
{
    // console.info("ProcessModeConfigurationData: Start");

    // determine the type of in/output that has been selected and populate the form
    var TypeOfChannelId = parseInt($('#' + ChannelType +  channelId + " option:selected").val(), 10);
    var channelConfigSet = JsonConfig.channels[channelId];

    if (isNaN(TypeOfChannelId))
    {
        // use the value we got from the controller
        TypeOfChannelId = channelConfigSet.type;
    }
    var channelConfig = channelConfigSet[TypeOfChannelId];
    var ChannelTypeName = channelConfig.type.toLowerCase();
    ChannelTypeName = ChannelTypeName.replace(".", "_");
    ChannelTypeName = ChannelTypeName.replace(" ", "_");
    // console.info("ChannelTypeName: " + ChannelTypeName);

    var elementids = [];
    var modeControlName = '#' + ChannelType + 'mode' + channelId;
    elementids = $(modeControlName + ' *[id]').filter(":input").map(function ()
    {
        return $(this).attr('id');
    }).get();

    elementids.forEach(function (elementid)
    {
        var SelectedElement = modeControlName + ' #' + elementid;
        if ($(SelectedElement).is(':checkbox'))
        {
            $(SelectedElement).prop('checked', channelConfig[elementid]);
        }
        else
        {
            $(SelectedElement).val(channelConfig[elementid]);
        }
    });

    if ("fpp_remote" === ChannelTypeName)
    {
        if (null !== Fseq_File_List)
        {
            ProcessModeConfigurationDatafppremote(channelConfig);
        }
    }

    else if ("effects" === ChannelTypeName)
    {
        ProcessModeConfigurationDataEffects(channelConfig);
    }

    else if ("relay" === ChannelTypeName)
    {
        // console.info("ProcessModeConfigurationData: relay");
        ProcessModeConfigurationDataRelay(channelConfig);
    }

    else if ("servo_pca9685" === ChannelTypeName)
    {
        // console.info("ProcessModeConfigurationData: servo");
        ProcessModeConfigurationDataServoPCA9685(channelConfig);
    }

    UpdateAdvancedOptionsMode();

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
        CreateOptionsFromConfig("output", Output_Config);
    }

    // is this an input config?
    else if (JsonConfigData.hasOwnProperty("input_config"))
    {
        // save the config for later use.
        Input_Config = JsonConfigData.input_config;
        CreateOptionsFromConfig("input", Input_Config);
    }

    // is this a device config?
    else if (JsonConfigData.hasOwnProperty("device"))
    {
        Device_Config = JsonConfigData.device;
        updateFromJSON(JsonConfigData);

        if (false === JsonConfigData.SdCardPresent) {
            $("#li-filemanagement").addClass("hidden");
        }
        else {
            $("#li-filemanagement").removeClass("hidden");
		}

        // is this a network config?
        if (JsonConfigData.hasOwnProperty("network")) {
            Network_Config = JsonConfigData.network;
            updateFromJSON(JsonConfigData);
        }
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

function GenerateInputOutputControlLabel(OptionListName, DisplayedChannelId)
{
    var Id = parseInt(DisplayedChannelId) + 1;
    var NewName = OptionListName.charAt(0).toUpperCase() + OptionListName.slice(1) + ": " + Id;

    return NewName;

} // GenerateInputOutputControlLabel

function LoadDeviceSetupSelectedOption(OptionListName, DisplayedChannelId )
{
    // console.info("OptionListName: " + OptionListName);
    // console.info("DisplayedChannelId: " + DisplayedChannelId);

    var HtmlLoadFileName = $('#' + OptionListName + DisplayedChannelId + ' option:selected').text().toLowerCase();
    // console.info("Base HtmlLoadFileName: " + HtmlLoadFileName);
    HtmlLoadFileName = HtmlLoadFileName.replace(".", "_");
    HtmlLoadFileName = HtmlLoadFileName.replace(" ", "_");
    HtmlLoadFileName = HtmlLoadFileName + ".html";
    // console.info("Adjusted HtmlLoadFileName: " + HtmlLoadFileName);

//TODO: Handle this better for items which don't require config pages - alexa, ddp, etc...

    if ("disabled.html" === HtmlLoadFileName)
    {
        $('#' + OptionListName + 'mode' + DisplayedChannelId).empty();
    }
    else
    {
        // try to load the field definition file for this channel type
        $('#' + OptionListName + 'mode' + DisplayedChannelId).load(HtmlLoadFileName, function ()
        {
            if ("input" === OptionListName)
            {
                ProcessInputConfig();
                ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Input_Config);
            }
            else if ("output" === OptionListName)
            {
                ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Output_Config);
            }
        });
    }

} // LoadDeviceSetupSelectedOption

function CreateOptionsFromConfig(OptionListName, Config)
{
    // console.info("CreateOptionsFromConfig");

    var Channels = Config.channels;

    if ("input" === OptionListName)
    {
        $('#ecpin').val(Config.ecpin);
    }

    // for each field we need to populate (input vs output)
    Object.keys(Channels).forEach(function (ChannelId)
    {
        // OptionListName is 'input' or 'output'
        // console.info("ChannelId: " + ChannelId);
        var CurrentChannel = Channels[ChannelId];

        // does the selection box we need already exist?
        if (!$('#' + OptionListName + 'mode' + ChannelId).length)
        {
            // create the selection box
            $('#fg_' + OptionListName).append('<label class="control-label col-sm-2" for="' + OptionListName + ChannelId + '">' + GenerateInputOutputControlLabel(OptionListName, ChannelId) + ' Mode</label>');
            $('#fg_' + OptionListName).append('<div class="col-sm-2"><select class="form-control wsopt" id="' + OptionListName + ChannelId + '"></select></div>');
            $('#fg_' + OptionListName + '_mode').append('<fieldset id="' + OptionListName + 'mode' + ChannelId + '"></fieldset>');
        }

        var jqSelector = "#" + OptionListName + ChannelId;

        // remove the existing options
        $(jqSelector).empty();

        // for each Channel type in the list
        Object.keys(CurrentChannel).forEach(function (SelectionTypeId)
        {
            // console.info("SelectionId: " + SelectionTypeId);
            if ("type" === SelectionTypeId)
            {
                // console.info("Set the selector type to: " + CurrentChannel.type);
                $(jqSelector).val(CurrentChannel.type);
                LoadDeviceSetupSelectedOption(OptionListName, ChannelId);
                $(jqSelector).change(function ()
                {
                    // console.info("Set the selector type to: " + CurrentChannel.type);
                    LoadDeviceSetupSelectedOption(OptionListName, ChannelId);
                });
            }
            else
            {
                var CurrentSection = CurrentChannel[SelectionTypeId];
                // console.info("Add '" + CurrentSection.type + "' to selector");
                $(jqSelector).append('<option value="' + SelectionTypeId + '">' + CurrentSection.type + '</option>');
            }
        }); // end for each selection type
    }); // end for each channel
} // CreateOptionsFromConfig

// Builds JSON config submission for "WiFi" tab
function ExtractNetworkConfigFromHtmlPage()
{
    Network_Config.ssid        = $('#ssid').val();
    Network_Config.passphrase  = $('#passphrase').val();
    Network_Config.hostname    = $('#hostname').val();
    Network_Config.sta_timeout = $('#sta_timeout').val();
    Network_Config.ip          = $('#ip').val();
    Network_Config.netmask     = $('#netmask').val();
    Network_Config.gateway     = $('#gateway').val();
    Network_Config.dhcp        = $('#dhcp').prop('checked');
    Network_Config.ap_fallback = $('#ap_fallback').prop('checked');
    Network_Config.ap_reboot   = $('#ap_reboot').prop('checked');
    Network_Config.ap_timeout  = $('#apt').prop('checked');
} // ExtractNetworkConfigFromHtmlPage

// Builds JSON config submission for "WiFi" tab
function submitWiFiConfig() {
    ExtractNetworkConfigFromHtmlPage();
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'device': Device_Config, 'network': Network_Config } } }));

} // submitWiFiConfig

function ExtractChannelConfigFromHtmlPage(JsonConfig, SectionName)
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

        if ((ChannelConfig.type === "Relay") && ($("#relaychannelconfigurationtable").length))
        {
            ChannelConfig.updateinterval = parseInt($('#updateinterval').val(), 10);
            $.each(ChannelConfig.channels, function (i, CurrentChannelConfig) {
                // console.info("Current Channel Id = " + CurrentChannelConfig.id);
                var currentChannelRowId = CurrentChannelConfig.id + 1;
                CurrentChannelConfig.en   = $('#Enabled_' + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.inv  = $('#Inverted_' + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.gid  = parseInt($('#gpioId_' + (currentChannelRowId)).val(), 10);
                CurrentChannelConfig.trig = parseInt($('#threshhold_' + (currentChannelRowId)).val(), 10);
            });
        }
        else if ((ChannelConfig.type === "Servo PCA9685") && ($("#servo_pca9685channelconfigurationtable").length))
        {
            ChannelConfig.updateinterval = parseInt($('#updateinterval').val(), 10);
            $.each(ChannelConfig.channels, function (i, CurrentChannelConfig) {
                // console.info("Current Channel Id = " + CurrentChannelConfig.id);
                var currentChannelRowId = CurrentChannelConfig.id + 1;
                CurrentChannelConfig.en  = $('#ServoEnabled_' + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.Min = parseInt($('#ServoMinLevel_' + (currentChannelRowId)).val(), 10);
                CurrentChannelConfig.Max = parseInt($('#ServoMaxLevel_' + (currentChannelRowId)).val(), 10);
                var ServoDataType        = parseInt($('#ServoDataType_' + (currentChannelRowId)).val(), 10);

                CurrentChannelConfig.rev = (ServoDataType & 0x01) ? true : false;
                CurrentChannelConfig.sca = (ServoDataType & 0x02) ? true : false;
                CurrentChannelConfig.b16 = (ServoDataType & 0x04) ? true : false;
            });
        }
        else
        {
            elementids.forEach(function (elementid) {
                var SelectedElement = modeControlName + ' #' + elementid;
                if ($(SelectedElement).is(':checkbox')) {
                    ChannelConfig[elementid] = $(SelectedElement).prop('checked');
                }
                else {
                    ChannelConfig[elementid] = $(SelectedElement).val();
                }
            });
        }
    }); // end for each channel

} // ExtractChannelConfigFromHtmlPage

function ValidateConfigFields(ElementList)
{
    // return true if errors were found
    var response = false;

    for (var ChildElementId = 0;
         ChildElementId < ElementList.length;
         ChildElementId++)
    {
        var ChildElement = ElementList[ChildElementId];
        // var ChildType = ChildElement.type;

        if ((ChildElement.validity.valid !== undefined) && (!$(ChildElement).hasClass('hidden')))
        {
            // console.info("ChildElement.validity.valid: " + ChildElement.validity.valid);
            if (false === ChildElement.validity.valid)
            {
                // console.info("          Element: " + ChildElement.id);
                // console.info("   ChildElementId: " + ChildElementId);
                // console.info("ChildElement Type: " + ChildType);
                response = true;
            }
        }
    }
    return response;

} // ValidateConfigFields

// Build dynamic JSON config submission for "Device" tab
function submitDeviceConfig()
{
    ExtractChannelConfigFromHtmlPage(Input_Config.channels, "input");
    Input_Config.ecb.enabled  = $("#ecb_enable").is(':checked');
    Input_Config.ecb.id       = $("#ecb_gpioid").val();
    Input_Config.ecb.polarity = $("#ecb_polarity").val();

    ExtractChannelConfigFromHtmlPage(Output_Config.channels, "output");

    Device_Config.id        = $('#config #device #id').val();
    Device_Config.miso_pin  = $('#config #device #miso_pin').val();
    Device_Config.mosi_pin  = $('#config #device #mosi_pin').val();
    Device_Config.clock_pin = $('#config #device #clock_pin').val();
    Device_Config.cs_pin    = $('#config #device #cs_pin').val();

    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'device': Device_Config, 'network': Network_Config } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'input':  { 'input_config': Input_Config } } } }));
    wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'output': { 'output_config': Output_Config } } } }));

} // submitDeviceConfig

function convertUTCDateToLocalDate(date)
{
    date = new Date(date);
    var localOffset = date.getTimezoneOffset() * 60000;
    var localTime = date.getTime();
    date = localTime - localOffset;

    return date;
} // convertUTCDateToLocalDate

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

        // target = "192.168.10.215";
        // target = "192.168.10.193";

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
            wsEnqueue(JSON.stringify({ 'cmd': { 'set': { 'time': { 'time_t': convertUTCDateToLocalDate(Date())/1000 } } } }));
            wsEnqueue(JSON.stringify({ 'cmd': { 'get': 'device' } })); // Get network config

            ProcessWindowChange($(location).attr("hash"));

            RequestStatusUpdate();                                                       // start self filling status loop
        };

        ws.onmessage = function (event)
        {
            // console.info("ws.onmessage: Start");
            if (typeof event.data === "string")
            {
                // console.info("ws.onmessage: Received: " + event.data);

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
                    console.info("ws.onmessage: Received: " + event.data);
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
                        snackSave();
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
        console.info('WS sending ' + OutputMessage);
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
    var maxDisplay = 0;

    if ($("#diag #viewStyle option:selected").val() === "rgb")
    {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)) * 3);
        for (var i = 0; i < maxDisplay; i += 3)
        {
            ctx.fillStyle = 'rgb(' + streamData[i + 0] + ',' + streamData[i + 1] + ',' + streamData[i + 2] + ')';
            var col = (i / 3) % cols;
            var row = Math.floor((i / 3) / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 1, size - 1);
        }
    }
    else     if ($("#diag #viewStyle option:selected").val() === "rgbw")
    {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)) * 4);
        for (i = 0; i < maxDisplay; i += 4)
        {
            var WhiteLevel = streamData[i + 3];
            ctx.fillStyle = 'rgb(' + Math.max(streamData[i + 0], WhiteLevel) + ',' + Math.max(streamData[i + 1], WhiteLevel) + ',' + Math.max(streamData[i + 2], WhiteLevel) + ')';
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
    var ParsedJsonAdmin = JSON.parse(data);
    AdminInfo = ParsedJsonAdmin.admin;

    $('#version').text(AdminInfo.version);
    $('#built').text(AdminInfo.built);
    $('#usedflashsize').text(AdminInfo.usedflashsize);
    $('#realflashsize').text(AdminInfo.realflashsize);
    $('#flashchipid').text(AdminInfo.flashchipid);

} // ProcessRecievedJsonAdminMessage

// ProcessRecievedJsonStatusMessage
function ProcessRecievedJsonStatusMessage(data)
{
    console.info("Status: " + data);

    var JsonStat = JSON.parse(data);
    var Status  = JsonStat.status;
    var System  = Status.system;
    var rssi    = System.rssi;
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
        $('#E131Status').removeClass("hidden")

        $('#uni_first').text(InputStatus.e131.unifirst);
        $('#uni_last').text (InputStatus.e131.unilast);
        $('#pkts').text     (InputStatus.e131.num_packets);
        $('#chanlim').text  (InputStatus.e131.unichanlim);
        $('#perr').text     (InputStatus.e131.packet_errors);
        $('#clientip').text (InputStatus.e131.last_clientIP);
    }
    else
    {
        $('#E131Status').addClass("hidden")
    }

    if (Status.input[0].hasOwnProperty('Artnet')) {
        $('#ArtnetStatus').removeClass("hidden")

        $('#an_uni_first').text(InputStatus.Artnet.unifirst);
        $('#an_uni_last').text(InputStatus.Artnet.unilast);
        $('#an_pkts').text(InputStatus.Artnet.num_packets);
        $('#an_chanlim').text(InputStatus.Artnet.unichanlim);
        $('#an_perr').text(InputStatus.Artnet.packet_errors);
        $('#an_clientip').text(InputStatus.Artnet.last_clientIP);
    }
    else {
        $('#ArtnetStatus').addClass("hidden")
    }

    if (Status.input[0].hasOwnProperty('ddp'))
    {
        $('#ddpStatus').removeClass("hidden")

        $('#ddppacketsreceived').text(InputStatus.ddp.packetsreceived);
        $('#ddpbytesreceived').text(InputStatus.ddp.bytesreceived);
        $('#ddperrors').text(InputStatus.ddp.errors);
    }
    else
    {
        $('#ddpStatus').addClass("hidden")
    }

    if (Status.system.hasOwnProperty('FPPDiscovery'))
    {
        $('#FPPRemoteStatus').removeClass("hidden")

        var FPPDstatus = Status.system.FPPDiscovery

        $('#fppsyncreceived').text(FPPDstatus.SyncCount);
        $('#fppsyncadjustments').text(FPPDstatus.SyncAdjustmentCount);
        $('#fppremoteip').text(FPPDstatus.FppRemoteIp);

        $('#fppremoteFilePlayerFilename').text(FPPDstatus.current_sequence);
        $('#fppremoteFilePlayerTimeElapsed').text(FPPDstatus.time_elapsed);
        $('#fppremoteFilePlayerTimeRemaining').text(FPPDstatus.time_remaining);
        $('#fppremoteFilePlayerTimeOffset').text(FPPDstatus.TimeOffset);
    }
    else
    {
        $('#FPPRemoteStatus').addClass("hidden")
    }

    if (Status.input[0].hasOwnProperty('LocalPlayer'))
    {
        var LocalPlayerStatus = Status.input[0].LocalPlayer;

        if (LocalPlayerStatus.hasOwnProperty('PlayList'))
        {
            $('#LocalPlayListPlayerStatus').removeClass("hidden");

            LocalPlayerStatus = LocalPlayerStatus.PlayList;
            
            $('#localPlayListName').text(LocalPlayerStatus.name);
            $('#localPlayListEntry').text(LocalPlayerStatus.entry);
            $('#localPlayListCount').text(LocalPlayerStatus.count);

            if (LocalPlayerStatus.hasOwnProperty('File'))
            {
                $('#localPlayListRepeat').text(LocalPlayerStatus.repeat);
            }
            else
            {
                $('#localPlayListRepeat').text("0");
            }
        }
        else
        {
            $('#LocalPlayListPlayerStatus').addClass("hidden")
        }

        if (LocalPlayerStatus.hasOwnProperty('File')) {
            $('#LocalFilePlayerStatus').removeClass("hidden");

            $('#localFilePlayerFilename').text(LocalPlayerStatus.File.current_sequence);
            $('#localFilePlayerTimeElapsed').text(LocalPlayerStatus.File.time_elapsed);
            $('#localFilePlayerTimeRemaining').text(LocalPlayerStatus.File.time_remaining);
        }
        else
        {
            $('#LocalFilePlayerStatus').addClass("hidden")
        }

        if (LocalPlayerStatus.hasOwnProperty('Effect'))
        {
            $('#LocalEffectPlayerStatus').removeClass("hidden");

            $('#localFilePlayerEffectName').text(LocalPlayerStatus.Effect.currenteffect);
            $('#localFilePlayerEffectTimeRemaining').text(LocalPlayerStatus.Effect.TimeRemaining);
        }
        else
        {
            $('#LocalEffectPlayerStatus').addClass("hidden")
        }

        if (LocalPlayerStatus.hasOwnProperty('Paused')) {
            $('#PausedPlayerStatus').removeClass("hidden");

            $('#PausedTimeRemaining').text(LocalPlayerStatus.Paused.TimeRemaining);
        }
        else {
            $('#PausedPlayerStatus').addClass("hidden")
        }
    }
    else
    {
        $('#LocalPlayListPlayerStatus').addClass("hidden")
        $('#LocalFilePlayerStatus').addClass("hidden")
        $('#LocalEffectPlayerStatus').addClass("hidden")
        $('#PausedPlayerStatus').addClass("hidden")
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
    $("#EfuProgressBar").addClass("hidden");
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

// reset config
function factoryReset()
{
    showReboot();
    wsEnqueue('X7');
} // factoryReset

