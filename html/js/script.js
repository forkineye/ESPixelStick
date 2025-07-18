var StatusRequestTimer = null;
var DiagTimer = null;
var MonitorTransactionRequestInProgressId = 0;
var PreviousMonitorTransactionRequestInProgressId = 0;

// global data
var AdminInfo = null;
var Output_Config = null; // Output Manager configuration record
var Input_Config = null; // Input Manager configuration record
var System_Config = null;
var Fseq_File_List = [];
var selector = [];
var ajaxQueue = $({}); // Create an empty jQuery object to act as a queue

var SdCardIsInstalled = false;
var FseqFileTransferStartTime = new Date();
var ServerTransactionTimer = null;
var FailedToCompleteServerTransaction = 0;
var DocumentIsHidden = false;

// Drawing canvas - move to diagnostics
var canvas = document.getElementById("canvas");
var ctx = canvas.getContext("2d");
ctx.font = "20px Arial";
ctx.textAlign = "center";

const rssiToPercent =
[
    /* -100 */ 0,
    /* -99 */ 0,
    /* -98 */ 0,
    /* -97 */ 0,
    /* -96 */ 0,
    /* -95 */ 0,
    /* -94 */ 4,
    /* -93 */ 6,
    /* -92 */ 8,
    /* -91 */ 11,
    /* -90 */ 13,
    /* -89 */ 15,
    /* -88 */ 17,
    /* -87 */ 19,
    /* -86 */ 21,
    /* -85 */ 23,
    /* -84 */ 26,
    /* -83 */ 28,
    /* -82 */ 30,
    /* -81 */ 32,
    /* -80 */ 34,
    /* -79 */ 35,
    /* -78 */ 37,
    /* -77 */ 39,
    /* -76 */ 41,
    /* -75 */ 43,
    /* -74 */ 45,
    /* -73 */ 46,
    /* -72 */ 48,
    /* -71 */ 50,
    /* -70 */ 52,
    /* -69 */ 53,
    /* -68 */ 55,
    /* -67 */ 56,
    /* -66 */ 58,
    /* -65 */ 59,
    /* -64 */ 61,
    /* -63 */ 62,
    /* -62 */ 64,
    /* -61 */ 65,
    /* -60 */ 67,
    /* -59 */ 68,
    /* -58 */ 69,
    /* -57 */ 71,
    /* -56 */ 72,
    /* -55 */ 73,
    /* -54 */ 75,
    /* -53 */ 76,
    /* -52 */ 77,
    /* -51 */ 78,
    /* -50 */ 79,
    /* -49 */ 80,
    /* -48 */ 81,
    /* -47 */ 82,
    /* -46 */ 83,
    /* -45 */ 84,
    /* -44 */ 85,
    /* -43 */ 86,
    /* -42 */ 87,
    /* -41 */ 88,
    /* -40 */ 89,
    /* -39 */ 90,
    /* -38 */ 90,
    /* -37 */ 91,
    /* -36 */ 92,
    /* -35 */ 93,
    /* -34 */ 93,
    /* -33 */ 94,
    /* -32 */ 95,
    /* -31 */ 95,
    /* -30 */ 96,
    /* -29 */ 96,
    /* -28 */ 97,
    /* -27 */ 97,
    /* -26 */ 98,
    /* -25 */ 98,
    /* -24 */ 99,
    /* -23 */ 99,
    /* -22 */ 99,
    /* -21 */ 100,
    /* -20 */ 100,
    /* -19 */ 100,
    /* -18 */ 100,
    /* -17 */ 100,
    /* -16 */ 100,
    /* -15 */ 100,
    /* -14 */ 100,
    /* -13 */ 100,
    /* -12 */ 100,
    /* -11 */ 100,
    /* -10 */ 100,
    /* -9 */  100,
    /* -8 */  100,
    /* -7 */  100,
    /* -6 */  100,
    /* -5 */  100,
    /* -4 */  100,
    /* -3 */  100,
    /* -2 */  100,
    /* -1 */  100,
    /*  0 */  100,
];

// Default modal properties
$.fn.modal.Constructor.DEFAULTS.backdrop = 'static';
$.fn.modal.Constructor.DEFAULTS.keyboard = false;

// jQuery doc ready
$(function ()
{
    $.ajaxQueue = function(ajaxOpts)
    {
        // hold the original complete function
        var oldComplete = ajaxOpts.complete;

        // queue our ajax request
        ajaxQueue.queue(function(next)
        {
            // create a complete callback to fire the next event in the queue
            ajaxOpts.complete = function()
            {
                // fire the original complete if it was there
                if (oldComplete) oldComplete.apply(this, arguments);
                next(); // run the next query in the queue
            };

            // run the query
            $.ajax(ajaxOpts);
        });
    };

    // Menu navigation for single page layout
    $('ul.navbar-nav li a').on("click", (function ()
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
        $('#efu').on("change" ,function ()
        {
            let file = _('efu').files[0];
            let formdata = new FormData();
            formdata.append("file", file);
            let FileXfer = new XMLHttpRequest();

            FileXfer.upload.addEventListener("progress", progressHandler, false);
            FileXfer.addEventListener("load", completeHandler, false);
            FileXfer.addEventListener("error", errorHandler, false);
            FileXfer.addEventListener("abort", abortHandler, false);
            FileXfer.open("POST", "updatefw");
            FileXfer.send(formdata);
            $("#EfuProgressBar").removeClass("hidden");

            function _(el) {
                return document.getElementById(el);
            }
            function progressHandler(event) {
                let percent = (event.loaded / event.total) * 100;
                _("EfuProgressBar").value = Math.round(percent);
            }

            function completeHandler(event) {
                console.log("Transfer Complete");
                // console.debug("event: '" + event + "'");
                // console.debug("event.target.response: '" + event.target.response + "'");
                // console.debug("event.target.responseText: '" + event.target.responseText + "'");
                // console.debug("event.target.status : '" + event.target.status + "'");
                // console.debug("event.target.statusText : '" + event.target.statusText + "'");

                // _("status").innerHTML = event.target.responseText;
                _("EfuProgressBar").value = 0; //will clear progress bar after successful upload
                $("#EfuProgressBar").addClass("hidden");

                if(event.target.status === 200)
                {
                    alert("Firmware Upload SUCCESS!");
                    reboot();
                }
                else
                {
                    alert("Firmware Upload FAILED!\n" + event.target.response);
                    reboot();
                }
            }

            function errorHandler(event) {
                console.error("Transfer Error");
                $("#EfuProgressBar").addClass("hidden");
                alert("Firmware Upload FAILED!");
            }

            function abortHandler(event) {
                console.error("Transfer Abort");
                $("#EfuProgressBar").addClass("hidden");
                alert("Firmware Upload FAILED!");
            }
        });
    }));

    // DHCP field toggles
    $('#wifi #dhcp').on("change", (function () {
        if ($(this).is(':checked')) {
            $('.wifiDhcp').addClass('hidden');
        }
        else {
            $('.wifiDhcp').removeClass('hidden');
        }
        $('#btn_network').prop("disabled", ValidateConfigFields($("#network #wifi input")));
    }));

    $('#eth #dhcp').on("change", (function () {
        if ($(this).is(':checked')) {
            $('.ethdhcp').addClass('hidden');
        }
        else {
            $('.ethdhcp').removeClass('hidden');
        }
        $('#btn_network').prop("disabled", ValidateConfigFields($("#network #wifi input")));
    }));

    $('#network').on("input", (function () {
        $('#btn_network').prop("disabled", ValidateConfigFields($("#network #wifi input")));
    }));

    $('#config').on("input", (function () {
        $('#DeviceConfigSave').prop("disabled", ValidateConfigFields($('#config input')));
    }));

    $('#DeviceConfigSave').on("click", (function () {
        submitDeviceConfig();
    }));

    $('#btn_network').on("click", (function () {
        submitNetworkConfig();
    }));

    $('#diag #viewStyle').on("change", (function () {
        $.cookie('diagviewStyle', $('#diag #viewStyle').val());
        clearStream();
    }));
    if(undefined !== $.cookie('diagviewStyle'))
    {
        $('#diag #viewStyle').val($.cookie('diagviewStyle'));
    }

    $('#v_columns').on('input', function () {
        $.cookie('DiagColumns', parseInt($('#v_columns').val()));
        clearStream();
    });
    if(undefined !== $.cookie('DiagColumns'))
    {
        // let NumColumns = $.cookie('DiagColumns');
        $('#v_columns').val($.cookie('DiagColumns'));
    }

    //TODO: This should pull a configuration from the stick and not the web interface as web data could be invalid
    $('#backupconfig').on("click", (function () {

        let TotalConfig = JSON.stringify({ 'system': System_Config, 'input': Input_Config, 'output': Output_Config });

        let blob = new Blob([TotalConfig], { type: "text/json;charset=utf-8" });
        let FileName = System_Config.device.id.replace(".", "-").replace(" ", "-").replace(",", "-") + "-" + AdminInfo.flashchipid;
        saveAs(blob, FileName + ".json"); // Filesaver.js
    }));

    $('#restoreconfig').on("change", (function ()
    {
        if (this.files.length !== 0)
        {
            const reader = new FileReader();
            reader.onload = function fileReadCompleted()
            {
                // when the reader is done, the content is in reader.result.
                ProcessConfigFromLocalFile(reader.result);
            };
            reader.readAsText(this.files[0]);
        }
    }));

    $('#adminReboot').on("click", (function () {
        reboot();
    }));

    $('#AdvancedOptions').on("change", (function () {
	    $.cookie('advancedMode', $('#AdvancedOptions').prop("checked"), { expires: 365 });
        UpdateAdvancedOptionsMode();
        UpdateChannelCounts();
    }));
    if(undefined !== $.cookie('advancedMode'))
    {
        $('#AdvancedOptions').prop("checked", $.cookie('advancedMode') === "false" ? false : true);
    }

    let finalUrl = "upload";
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
            acceptedFiles: '.fseq,.pl,.zip,.xlz',
            timeout: 99999999, /*milliseconds = 27 days*/
            init: function ()
            {
                this.on('success', function (file) {
                    // console.log("Success");
                    // console.log("File: " + file.name);
                    Dropzone.forElement('#filemanagementupload').removeAllFiles(true)
                    RequestListOfFiles();
                });

                this.on('complete', function (file) {
                    // console.log("complete");
                    // console.log("File: " + file.name);
                    $('#fseqprogress_fg').addClass("hidden");
                    let extensionPosition = file.name.lastIndexOf(".");
                    if(extensionPosition !== -1)
                    {
                        let extension = file.name.toLowerCase().slice(extensionPosition + 1);
                        if(("zip" === extension) || ("xlz" === extension))
                        {
                            let RebootNow = confirm("Downloaded a zip file. Unzip requires a reboot.\nClick 'OK' to Reboot NOW.");
                            if(RebootNow)
                            {
                                reboot();
                            }
                        }
                    }
                    // let DeltaTime = (new Date().getTime() - FseqFileTransferStartTime.getTime()) / 1000;
                    // let rate = Math.floor((file.size / DeltaTime) / 1000);
                    // console.debug("Final Transfer Rate: " + rate + "KBps");
                });

                this.on('addedfile', function (file) {
                    // console.log("addedfile");
                    // console.log("File: " + file.name);
                    FseqFileTransferStartTime = new Date();
                });

                this.on('uploadprogress', function (file, percentProgress, bytesSent) {
                    // console.log("percentProgress: " + percentProgress);
                    // console.log("bytesSent: " + bytesSent);
                    $('#fseqprogress_fg').removeClass("hidden");
                    $('#fseqprogressbytes').html(bytesSent);

                    let now = new Date().getTime();
                    let DeltaTime = (now - FseqFileTransferStartTime.getTime()) / 1000;
                    let rate = Math.floor((bytesSent / DeltaTime) / 1000);
                    $('#fseqprogressrate').html(rate + "KBps");
                });

                this.on('error', function (file, msg) {
                    // console.log("error");
                    // console.log("File: " + file.name);
                    // console.log("msg: " + msg);
                    alert("Failed uploading '" + file.name + "'\nError: '" + msg + "'");
                });

                this.on('sending', function (file, xhrObject)
                {
                    // console.log("sending");
                    // console.log("File: " + file.name);
                    // console.log("resp: " + xhrObject);

                    xhrObject.addEventListener("error", function (event)
                    {
                        // console.error("TCP Error: " + event.type);
                        alert("File Transfer Error: " + event.type);
                    });

                    xhrObject.addEventListener("abort", function (event)
                    {
                        // console.error("TCP Abort: " + event.type);
                        alert("File Transfer Error: " + event.type);
                    });
                });

                this.on('queuecomplete', function () {
                    console.log("queuecomplete");
                });
            },
        });

    $("#filemanagementupload").addClass("dropzone");

    $('#FileDeleteButton').on("click", (function () {
        RequestFileDeletion();
    }));
    /*
        $('#FileUploadButton').on("click", (function () {
            RequestFileUpload();
        }));
    */
    // Autoload tab based on URL hash
    let hash = window.location.hash;
    hash && $('ul.navbar-nav li a[href="' + hash + '"]').click();

    // Halt server health check if document is not visible
    document.addEventListener("visibilitychange", function () {
        if (document.hidden) {
            DocumentIsHidden = true;
        } else {
            DocumentIsHidden = false;
            SetServerTime();
        }
    });

// lets get started
    RequestConfigFile("admininfo.json");
    RequestConfigFile("config.json");
    RequestConfigFile("output_config.json");
    RequestConfigFile("input_config.json");
    SetServerTime();
    RequestDiagData();
    StartRequestingStatusUpdate();  // start self filling status loop
    MonitorServerConnection();
});

function SetServerTime()
{
    // console.debug("SetServerTime");
    let CurrentDate = Math.floor((new Date()).getTime() / 1000);
    // console.debug("CurrentDate: " + CurrentDate);
    SendCommand('settime?newtime=' + (CurrentDate));
} // SetServerTime

function ProcessConfigFromLocalFile(data)
{
    // console.debug(data);
    let ParsedLocalConfig = JSON.parse(data);

    // merge the restored config into the existing config and save the results to the ESP
    MergeConfig(ParsedLocalConfig.system, System_Config, "config",        'system');
    MergeConfig(ParsedLocalConfig.output, Output_Config, "output_config", 'output_config');
    MergeConfig(ParsedLocalConfig.input,  Input_Config,  "input_config",  'input_config');

} // ProcessLocalConfig

/**
 * @param {any} SourceData
 * @param {any} TargetData
 * @param {string} FileName
 * @param {string} SectionName
 */
function MergeConfig(SourceData, TargetData, FileName, SectionName)
{
    // console.debug("SourceData: " + JSON.stringify(SourceData));
    // console.debug("TargetData: " + JSON.stringify(TargetData));

    let FinalSourceData = JSON.parse('{"' + SectionName + '":' + JSON.stringify(SourceData) + "}");
    let FinalTargetData = JSON.parse('{"' + SectionName + '":' + JSON.stringify(TargetData) + "}");

    // console.debug("FinalSourceData: " + JSON.stringify(FinalSourceData));
    // console.debug("FinalTargetData: " + JSON.stringify(FinalTargetData));

    MergeConfigTree(FinalSourceData, FinalTargetData, FinalTargetData, "");

    // let DataString = '{"' + SectionName + '":' + JSON.stringify(TargetData) + "}";
    SendConfigFileToServer(FileName, JSON.stringify(FinalTargetData));

} // MergeConfig

function JsonObjectAccess(obj, Path, value, Action)
{
    try
    {
        if(Array.isArray(Path) == false)
        {
            Path = [Path];
        }

        let level = 0;
        var Return_Value;
        Path.reduce((a, b)=>
        {
            level++;
            if (level === Path.length)
            {
                if(Action === 'Set')
                {
                    a[b] = value;
                    return value;
                }
                else if(Action === 'Get')
                {
                    Return_Value = a[b];
                }
                else if(Action === 'Unset')
                {
                    delete a[b];
                }
            }
            else
            {
                return a[b];
            }
        }, obj);
        return Return_Value;
    }

    catch(err)
    {
        console.error(err);
        return obj;
    }
} // JsonObjectAccess

function MergeConfigTree(SourceTree, TargetTree, CurrentTarget, FullSelector)
{
    // console.debug("Entry: FullSelector: '" + FullSelector + "'");

    // make sure the selector is an array
    if(!Array.isArray(FullSelector))
    {
        FullSelector = [FullSelector];
        // the first entry into this function has a
        // null selector value that needs to be removed
        if(FullSelector[0] === "")
        {
            FullSelector.pop();
        }
    }

    // Target drives the data.
    for (let CurrentElementName in CurrentTarget)
    {
        // console.debug("CurrentElement: " + CurrentElementName);
        // remember the path to this element
        let CurrentSelectorPath = JSON.parse(JSON.stringify(FullSelector));
        CurrentSelectorPath.push(CurrentElementName);

        // console.debug("CurrentSelectorPath: '" + CurrentSelectorPath + "'");
        let CurrentElementValue = CurrentTarget[CurrentElementName];

        if(CurrentElementValue === undefined)
        {
            console.error("target element is not properly formed");
            continue;
        }

        if (typeof CurrentElementValue === 'object')
        {
            // console.debug("take the current object apart");
            MergeConfigTree(SourceTree,
                            TargetTree,
                            CurrentElementValue,
                            CurrentSelectorPath);
        }
        else
        {
            let SourceLookupValue = JsonObjectAccess(SourceTree, CurrentSelectorPath, "", "Get");
            // let TargetLookupValue = JsonObjectAccess(TargetTree, CurrentSelectorPath, "", "Get");

            // does the source have this element?
            if(undefined !== SourceLookupValue)
            {
                // save the current object value
                // console.debug("Saving CurrentSelectorPath: '" + CurrentSelectorPath + "'");
                JsonObjectAccess(TargetTree, CurrentSelectorPath, SourceLookupValue, "Set");
            }
            else
            {
                console.error("NOT Saving CurrentElement. Source value is Not in target tree.");
            }
        }
    }
} // MergeConfigTree

function UpdateAdvancedOptionsMode(){
    // console.debug("UpdateAdvancedOptionsMode");

    let am = $('#AdvancedOptions');
    let AdvancedModeState = am.prop("checked");

    $(".AdvancedMode").each(function () {
        if (true === AdvancedModeState) {
            $(this).removeClass("hidden");
        }
        else {
            $(this).addClass("hidden");
        }
    });
} // UpdateAdvancedOptionsMode

function UpdateChannelCounts() {
    // console.debug("UpdateChannelCounts");
    if (null !== Output_Config) {
        $(".SerialCount").each(function () {
            $(this).attr('max', Output_Config.TotalChannels);
        });

        $(".PixelCount").each(function () {
            $(this).attr('max', Output_Config.TotalChannels / 3);
        });
    }
} // UpdateChannelCounts

function SendAllConfigFilesToServer()
{
    // console.debug("SendAllConfigFilesToServer");

    ConfigWaitMessageStart(3);
    SendConfigFileToServer("config", JSON.stringify({'system': System_Config}));
    SendConfigFileToServer("output_config", JSON.stringify({'output_config': Output_Config}));
    SendConfigFileToServer("input_config", JSON.stringify({'input_config': Input_Config}));

} // SendAllConfigFilesToServer

function SendConfigFileToServer(FileName, DataString)
{
    // console.debug("SendConfigFileToServer: FileName: " + FileName);
    // console.debug("SendConfigFileToServer: Data: " + JSON.stringify(DataString));
    let url = "conf/" + FileName + ".json";

    $.ajaxQueue(
    {
        type: "PUT",
        url: url,
        data: DataString,
        contentType: "application/json",
        mode: "cors", // no-cors, *cors, same-origin
        dataType: "json",
        timeout: 20000,
        async: false,
        headers: { 'Content-Type': 'application/json' },
        cache: "reload", // *default, no-cache, reload, force-cache, only-if-cached
        credentials: "same-origin", // include, *same-origin, omit
        redirect: "follow", // manual, *follow, error
        referrerPolicy: "no-referrer", // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin, same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
        success: function(response)
        {
            // console.debug("SendConfigFileToServer: " + JSON.stringify(response));
            ConfigWaitMessageEnd(0);
        },
        error: function()
        {
            console.error("SendConfigFileToServer: AJAX request failed for: " + url);
            ConfigWaitMessageEnd(1);
        }
    });
    // console.debug("SendConfigFileToServer: Done: FileName: " + FileName);

} // SendConfigFileToServer

function ProcessWindowChange(NextWindow) {

    if (NextWindow === "#diag") {
    }

    else if (NextWindow === "#admin")
    {
        RequestConfigFile("admininfo.json");
        RequestConfigFile("config.json");
        RequestConfigFile("output_config.json");
        RequestConfigFile("input_config.json");
    }

    else if ((NextWindow === "#pg_network") || (NextWindow === "#home"))
    {
        RequestConfigFile("admininfo.json");
        RequestConfigFile("config.json");
    }

    else if (NextWindow === "#config")
    {
        RequestListOfFiles();
        RequestConfigFile("admininfo.json");
        RequestConfigFile("config.json");
        RequestConfigFile("output_config.json");
        RequestConfigFile("input_config.json");
    }

    else if (NextWindow === "#filemanagement") {
        RequestListOfFiles();
    }

    UpdateAdvancedOptionsMode();
    UpdateChannelCounts();

} // ProcessWindowChange

function RequestDiagData()
{
    let NextTimePeriodMS = 100;
    if(null === DiagTimer)
    {
        DiagTimer = setInterval(function()
        {
            if ($('#diag').is(':visible'))
            {
                MonitorTransactionRequestInProgressId++;
                fetch("V1",
                {
                    method: "GET", // *GET, POST, PUT, DELETE, etc.
                    mode: "cors", // no-cors, *cors, same-origin
                    dataType: "json",
                    timeout: 20000,
                    async: false,
                    headers: { 'Content-Type': 'application/json' },
                    cache: "reload", // *default, no-cache, reload, force-cache, only-if-cached
                    credentials: "same-origin", // include, *same-origin, omit
                    redirect: "follow", // manual, *follow, error
                    referrerPolicy: "no-referrer", // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin, same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
                })
                .then(async webResponse =>
                {
                    const data = await webResponse.blob();
                    // console.debug("RequestDiagData:webResponse.status: " + webResponse.status);
                    // console.debug("RequestDiagData:webResponse.ok: " + webResponse.ok);
                    // check for error response
                    if (!webResponse.ok)
                    {
                        // get error message from body or default to response status
                        const error = webResponse.status;
                        console.error("SendCommand: Error: " + Promise.reject(error));
                        FailedToCompleteServerTransaction ++;
                    }
                    else
                    {
                        // console.debug("SendCommand: Transaction complete");
                        FailedToCompleteServerTransaction = 0;
                        let streamData = new Uint8Array(await data.arrayBuffer());
                        drawStream(streamData);
                    }
                })
                .catch(async error =>
                {
                    FailedToCompleteServerTransaction ++;
                    console.error('SendCommand: Error: ', error);
                });
            }
        }, NextTimePeriodMS);
    }
} // RequestDiagData

function RequestConfigFile(FileName)
{
    // console.debug("RequestConfigFile FileName: " + FileName);
    var url = "conf/" + FileName;
    console.info("RequestConfigFile: 'GET' URL: '" + url + "'");

    $.ajaxQueue(
    {
        type: "GET",
        url: url,
        mode: "cors", // no-cors, *cors, same-origin
        dataType: "json",
        timeout: 20000,
        async: false,
        headers: { 'Content-Type': 'application/json' },
        cache: "reload", // *default, no-cache, reload, force-cache, only-if-cached
        credentials: "same-origin", // include, *same-origin, omit
        redirect: "follow", // manual, *follow, error
        referrerPolicy: "no-referrer", // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin, same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
        success: function(response)
        {
            // console.debug("RequestConfigFile: " + JSON.stringify(response));
            ProcessReceivedJsonConfigMessage(response);
        },
        error: function()
        {
            console.error("RequestConfigFile: AJAX request failed for: " + url);
        }
    });

    // console.debug("RequestConfigFile - done. FileName: " + FileName);

} // RequestConfigFile

async function StartRequestingStatusUpdate()
{
    // console.log("StartRequestingStatusUpdate Start: ");
    // is the timer running?
    if (null === StatusRequestTimer)
    {
        RequestStatusUpdate();
        // timer runs forever
        StatusRequestTimer = setTimeout(function ()
        {
            clearTimeout(StatusRequestTimer);
            StatusRequestTimer = null;

            StartRequestingStatusUpdate();
        }, 1000);
    } // end timer was not running
} // StartRequestingStatusUpdate

function RequestStatusUpdate()
{
    if ($('#home').is(':visible'))
    {
        // ask for a status update from the server
        let FileName = "XJ";
        console.log("RequestStatusUpdate FileName: " + FileName);

        MonitorTransactionRequestInProgressId++;

        $.ajaxQueue(
        {
            type: "GET",
            url: FileName,
            mode: "cors", // no-cors, *cors, same-origin
            dataType: "json",
            timeout: 20000,
            async: false,
            headers: { 'Content-Type': 'application/json' },
            cache: "reload", // *default, no-cache, reload, force-cache, only-if-cached
            credentials: "same-origin", // include, *same-origin, omit
            redirect: "follow", // manual, *follow, error
            referrerPolicy: "no-referrer", // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin, same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
            success: function(response)
            {
                console.log("RequestStatusUpdate: " + JSON.stringify(response));
                FailedToCompleteServerTransaction = 0;
                ProcessReceivedJsonStatusMessage(response);
            },
            error: function(textStatus)
            {
                FailedToCompleteServerTransaction ++;
                console.error("Could not read Status file: " + FileName);
                console.error("Error:", textStatus);
            }
        });
    } // end home (aka status) is visible

} // RequestStatusUpdate

function RequestListOfFiles()
{
    // console.debug("ask for a file list from the server");

    var url = "fseqfilelist";
    // console.debug("RequestListOfFiles: 'GET' URL: '" + url + "'");

    MonitorTransactionRequestInProgressId++;
    $.ajaxQueue(
    {
        url: url,
        method: 'GET',
        mode: "cors", // no-cors, *cors, same-origin
        dataType: "json",
        timeout: 20000,
        async: false,
        headers: { 'Content-Type': 'application/json' },
        cache: "reload", // *default, no-cache, reload, force-cache, only-if-cached
        credentials: "same-origin", // include, *same-origin, omit
        redirect: "follow", // manual, *follow, error
        referrerPolicy: "no-referrer", // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin, same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
        success: function (webResponse)
        {
            if ({}.hasOwnProperty.call(webResponse, "SdCardPresent"))
            {
                // console.debug("RequestListOfFiles: " + JSON.stringify(webResponse));
                ProcessGetFileListResponse(webResponse);
            }
        },
        error: function (jqXHR, textStatus, errorThrown)
        {
            FailedToCompleteServerTransaction ++;
            console.error('RequestListOfFiles: Error: ', textStatus);
            console.error('RequestListOfFiles: errorThrown: ', errorThrown);
        }
    });

} // RequestListOfFiles

function BytesToMB(Value) {
     return  Math.round((Value / (1024 * 1024))).toLocaleString();

} // BytesToMB

async function ProcessGetFileListResponse(JsonData) {
    // console.debug("ProcessGetFileListResponse");

    SdCardIsInstalled = JsonData.SdCardPresent;
    // console.debug("SdCardIsInstalled: " + SdCardIsInstalled);

    if (true === SdCardIsInstalled)
    {
        $("#li-filemanagement").removeClass("hidden");
    }
    else
    {
        $("#li-filemanagement").addClass("hidden");
    }

    $("#totalBytes").val(BytesToMB(JsonData.totalBytes));
    $("#usedBytes").val(BytesToMB(JsonData.usedBytes));
    $("#remainingBytes").val(BytesToMB(JsonData.totalBytes - JsonData.usedBytes));
    $("#filecount").val(JsonData.numFiles);

    // console.debug("totalBytes: " + JsonConfigData.totalBytes);
    // console.debug("usedBytes: " + JsonConfigData.usedBytes);
    // console.debug("numFiles: " + JsonConfigData.numFiles);

    Fseq_File_List = [];

    // delete current entries
    $('#FileManagementTable').empty();

    JsonData.files.sort(function(a, b)
    {
        return a.name.localeCompare(b.name);
    });

    JsonData.files.forEach(function (file)
    {
        let CurrentRowId = $('#FileManagementTable > tr').length;
        let SelectedPattern = '<td><input  type="checkbox" id="FileSelected_' + (CurrentRowId) + '"></td>';
        let NamePattern = '<td><output type="text" id="FileName_' + (CurrentRowId) + '"></td>';
        let DatePattern = '<td><output type="text" id="FileDate_' + (CurrentRowId) + '"></td>';
        let SizePattern = '<td><output type="text" id="FileSize_' + (CurrentRowId) + '"></td>';

        let rowPattern = '<tr>' + SelectedPattern + NamePattern + DatePattern + SizePattern + '</tr>';
        $('#FileManagementTable').append(rowPattern);

        try {
            $('#FileName_' + (CurrentRowId)).val(file.name);
            $('#FileDate_' + (CurrentRowId)).val(new Date(file.date * 1000).toLocaleString());
            $('#FileSize_' + (CurrentRowId)).val(file.length.toLocaleString());
            Fseq_File_List.push(file);
        }
        catch
        {
            $('#FileName_' + (CurrentRowId)).val("InvalidFile");
            $('#FileDate_' + (CurrentRowId)).val(new Date(0).toISOString());
            $('#FileSize_' + (CurrentRowId)).val(0);
        }
    }); // end foreach

    SetServerTime();

} // ProcessGetFileListResponse

async function RequestFileDeletion() {

    $('#FileManagementTable > tr').each(function (CurRowId) {
        if (true === $('#FileSelected_' + CurRowId).prop("checked")) {
            let name = $('#FileName_' + CurRowId).val().toString();
            console.log("delete file: " + name);
            SendCommand('file/delete/' + name);
            setTimeout(function()
            {
                RequestListOfFiles();
            }, 5000);
            // console.debug("delete Response: " + Response);
        }
    });
} // RequestFileDeletion

function ProcessModeConfigurationDatafppremote(channelConfig) {
    let jqSelector = "#fseqfilename";

    // remove the existing options
    $(jqSelector).empty();

    $(jqSelector).append('<option value="...">Play Remote Sequence</option>');

    // for each file in the list
    Fseq_File_List.sort();
    Fseq_File_List.forEach(function (listEntry)
    {
        var LowerName = listEntry.name;
        LowerName = LowerName.toLowerCase();
        // console.debug("LowerName: " + LowerName);
        if((-1 !== LowerName.indexOf(".fseq")) ||
           (-1 !== LowerName.indexOf(".pl")))
        {
            // console.debug("add in a new entry");
            $(jqSelector).append('<option value="' + listEntry.name + '">' + listEntry.name + '</option>');
        }
    });

    // set the current selector value
    $(jqSelector).val(channelConfig.fseqfilename);

} // ProcessModeConfigurationDatafppremote

function ProcessModeConfigurationDataEffects(channelConfig) {
    let jqSelector = "#currenteffect";

    // remove the existing options
    $(jqSelector).empty();

    // for each option in the list
    channelConfig.effects.forEach(function (listEntry) {
        // add in a new entry
        $(jqSelector).append('<option value="' + listEntry.name + '">' + listEntry.name + '</option>');
    });

    $(jqSelector).val(channelConfig.currenteffect);
    SetEffectVisibility();

    // set the current selector value
    $(jqSelector).on("change", function()
    {
        SetEffectVisibility();
    });

    // set up the transitions table
    $('#TransitionColorTable tbody').removeData();

    channelConfig.transitions.forEach(element => {
        // console.debug("Element.r = " + element.r);
        // console.debug("Element.g = " + element.g);
        // console.debug("Element.b = " + element.b);

        let CurrentColor = "#" + ((element.r * 256 * 256) + (element.g * 256) + element.b).toString(16);
        transitionAddRow(CurrentColor);
    });

    RenumberTransitionTable();

    $('#AddTransitionBtn').off();
    $('#AddTransitionBtn').on("click", (function () {
        // console.debug("Add a transition");
        transitionAddRow('#000000');
        RenumberTransitionTable();
    }));

    // set up the marqueue groups table
    $('#MarqueeGroupTable tbody').removeData();

    channelConfig.MarqueeGroups.forEach(element => {
        // console.debug("Element.r = " + element.r);
        // console.debug("Element.g = " + element.g);
        // console.debug("Element.b = " + element.b);
        MarqueeGroupAddRow(element);
    });

    RenumberMarqueeGroupTable();

    $('#AddMarqueeGroupBtn').off();
    $('#AddMarqueeGroupBtn').on("click", (function () {
        // console.debug("Add a MarqueeGroup button pressed");
        let newMarqueeGroup = {};
        newMarqueeGroup.brightness = 50;
        newMarqueeGroup.brightnessEnd = 50;
        newMarqueeGroup.pixel_count = 5;
        newMarqueeGroup.color = {};
        newMarqueeGroup.color.r = 128;
        newMarqueeGroup.color.g = 128;
        newMarqueeGroup.color.b = 128;
        MarqueeGroupAddRow(newMarqueeGroup);
        RenumberMarqueeGroupTable();
    }));

} // ProcessModeConfigurationDataEffects

function SetEffectVisibility()
{
    let jqSelector = "#currenteffect";
    if($(jqSelector).val()==="Marquee")
    {
        $("#MarqueeConfig").removeClass("hidden");
        $("#TransitionsConfig").addClass("hidden");
    }
    else if($(jqSelector).val()==="Transition")
    {
        $("#MarqueeConfig").addClass("hidden");
        $("#TransitionsConfig").removeClass("hidden");
    }
    else
    {
        $("#MarqueeConfig").addClass("hidden");
        $("#TransitionsConfig").addClass("hidden");
    }
} // SetEffectVisibility

function UUID() {
    var uuid = (function () {
        var i,
            c = "89ab",
            u = [];
        for (i = 0; i < 36; i += 1) {
            u[i] = (Math.random() * 16 | 0).toString(16);
        }
        u[8] = u[13] = u[18] = u[23] = "-";
        u[14] = "4";
        u[19] = c.charAt(Math.random() * 4 | 0);
        return u.join("");
    })();
    return {
        toString: function () {
            return uuid;
        },
        valueOf: function () {
            return uuid;
        }
    };
} // UUID

function transitionAddRow(CurrentColor) {

    // console.debug("length " + $('#TransitionColorTable tbody tr').length);

    if (26 > $('#TransitionColorTable tbody tr').length) {
        let CurrentRowId = 'UUID_' + UUID().toString().toUpperCase();
        // console.debug("CurrentColor " + CurrentColor);

        while (-1 !== CurrentRowId.indexOf("-")) {
            CurrentRowId = CurrentRowId.replace("-", "_");
        }

        while (CurrentColor.length < 7) { CurrentColor = CurrentColor.replace("#", "#0"); }

        let TransitionIdPattern = '<td>' + (CurrentRowId) + '</td>';
        let TransitionColorPattern = '<td><input type="color" class="form-control is-valid" id="transitionColor_' + CurrentRowId + '" value="' + CurrentColor + '"></td>';
        let TransitionDeletePattern = '<td><button type="Button" class="btn btn-primary" RowId="' + CurrentRowId + '" id="transitionDelete_' + CurrentRowId + '">Delete</button></td>';
        let rowPattern = '<tr id="transitionRow_' + (CurrentRowId) + '" RowId="' + CurrentRowId + '"> ' + TransitionIdPattern + TransitionColorPattern + TransitionDeletePattern + '</tr> ';
        $('#TransitionColorTable tbody tr:last').after(rowPattern);
        $('#transitionDelete_' + CurrentRowId).on("click", (function () { transitionDeleteRow($(this)); }));
    }
} // transitionAddRow

function transitionDeleteRow(button) {

    let RowId = $(button).attr("RowId");
    // console.debug("Got Click for CurrentRowId: " + RowId);
    // console.debug("Length: " + $('#TransitionColorTable tbody tr').length);
    // 3 = hdr+2 rows
    if (3 < $('#TransitionColorTable tbody tr').length) {
        $('#transitionRow_' + RowId).remove();
        RenumberTransitionTable();
    }

} // transitionDeleteRow

function RenumberTransitionTable() {
    // renumber the table
    // console.debug("Length " + $('#TransitionColorTable tbody tr').length);

    $('#TransitionColorTable tbody tr').each(elementId => {
        if (0 !== elementId) {
            // console.debug(elementId);
            $('#TransitionColorTable tbody tr:eq(' + elementId + ') td:eq(0)').html((elementId).toString());
            if (4 > $('#TransitionColorTable tbody tr').length) {
                $('#TransitionColorTable tbody tr:eq(' + elementId + ') td:eq(2)').hide();
            }
            else {
                $('#TransitionColorTable tbody tr:eq(' + elementId + ') td:eq(2)').show();
            }
        }
    });

    if (26 > $('#TransitionColorTable tbody tr').length) {
        $('#AddTransitionBtn').show();
    }
    else {
        $('#AddTransitionBtn').hide();
    }

} // RenumberTransitionTable

function MarqueeGroupAddRow(CurrentConfig) {

    // console.debug("MarqueeGroupAddRow::MarqueeGroupTable length " + $('#MarqueeGroupTable tbody tr').length);
    // 1 header row + 5 group rows
    if (6 > $('#MarqueeGroupTable tbody tr').length) {
        let CurrentRowId = 'UUID_' + UUID().toString().toUpperCase();
        // console.debug("CurrentColor " + CurrentColor);

        let CurrentGroupColor = "#" + ((CurrentConfig.color.r * 256 * 256) + (CurrentConfig.color.g * 256) + CurrentConfig.color.b).toString(16);

        while (-1 !== CurrentRowId.indexOf("-")) 
        {
            CurrentRowId = CurrentRowId.replace("-", "_");
        }

        while (CurrentGroupColor.length < 7) { CurrentGroupColor = CurrentGroupColor.replace("#", "#0"); }

        let MarqueeGroupIdPattern           = '<td>' + (CurrentRowId) + '</td>';
        let MarqueeGroupIntensityPattern    = '<td><input  type="number" class="form-control is-valid" id="MarqueeGroupIntensity_'    + CurrentRowId + '" step="1" min="0" max="100" value=75></input></td>';
        let MarqueeGroupIntensityEndPattern = '<td><input  type="number" class="form-control is-valid" id="MarqueeGroupIntensityEnd_' + CurrentRowId + '" step="1" min="0" max="100" value=50></input></td>';
        let MarqueeGroupCountPattern        = '<td><input  type="number" class="form-control is-valid" id="MarqueeGroupCount_'        + CurrentRowId + '" step="1" min="0" value=25></input></td>';
        let MarqueeGroupColorPattern        = '<td><input  type="color"  class="form-control"          id="MarqueeGroupColor_'        + CurrentRowId + '" value=#999999></input></td>';
        let MarqueeGroupDeletePattern       = '<td><button type="Button" class="btn btn-primary"       id="MarqueeGroupDelete_'       + CurrentRowId + '" RowId="' + CurrentRowId + '">Delete</button></td>';

//      var rowPattern = '<tr>' + StartPattern + EndPattern + StartValuePattern + EndValuePattern + OutputPattern + '</tr>';
        let rowPattern = '<tr id="MarqueeGroupRow_' + (CurrentRowId) + '" RowId="' + CurrentRowId + '">' + 
                          MarqueeGroupIdPattern + 
                          MarqueeGroupIntensityPattern + 
                          MarqueeGroupIntensityEndPattern + 
                          MarqueeGroupCountPattern + 
                          MarqueeGroupColorPattern + 
                          MarqueeGroupDeletePattern + 
                          '</tr> ';
        $('#MarqueeGroupTable tbody tr:last').after(rowPattern);
        $('#MarqueeGroupDelete_' + CurrentRowId).on("click", function () { MarqueeGroupDeleteRow($(this)); });
        
        $('#MarqueeGroupIntensity_' + (CurrentRowId)).val(CurrentConfig.brightness);
        $('#MarqueeGroupIntensityEnd_' + (CurrentRowId)).val(CurrentConfig.brightnessEnd);
        $('#MarqueeGroupCount_' + (CurrentRowId)).val(CurrentConfig.pixel_count);
        $('#MarqueeGroupColor_' + (CurrentRowId)).val(CurrentGroupColor);
    }
} // MarqueeGroupAddRow

function MarqueeGroupDeleteRow(button) {
    // console.debug("MarqueeGroupDeleteRow::MarqueeGroupTable length " + $('#MarqueeGroupTable tbody tr').length);

    let RowId = $(button).attr("RowId");
    // console.debug("Got Click for CurrentRowId: " + RowId);
    // console.debug("Length: " + $('#MarqueeGroupTable tbody tr').length);
    // 3 = hdr+2 rows
    if (3 < $('#MarqueeGroupTable tbody tr').length) {
        $('#MarqueeGroupRow_' + RowId).remove();
        RenumberMarqueeGroupTable();
    }

} // MarqueeGroupDeleteRow

function RenumberMarqueeGroupTable() {
    // renumber the table
    // console.debug("RenumberMarqueeGroupTable::MarqueeGroupTable length " + $('#MarqueeGroupTable tbody tr').length);

    $('#MarqueeGroupTable tbody tr').each(elementId => {
        if (0 !== elementId) {
            // console.debug(elementId);
            $('#MarqueeGroupTable tbody tr:eq(' + elementId + ') td:eq(0)').html((elementId).toString());
        }
    });

    // 1 hdr + 5 rows
    if (6 > $('#MarqueeGroupTable tbody tr').length) {
        $('#AddMarqueeGroupBtn').show();
    }
    else {
        $('#AddMarqueeGroupBtn').hide();
    }

} // RenumberMarqueeGroupTable

function ProcessModeConfigurationDataRelay(RelayConfig) {
    // console.log("relaychannelconfigurationtable.rows.length = " + $('#relaychannelconfigurationtable tr').length);

    let ChannelConfigs = RelayConfig.channels;

    let HasPwmFrequency = false;
    if ({}.hasOwnProperty.call(ChannelConfigs[0], "Frequency")) {
        HasPwmFrequency = true;
        $("#Frequency_hr").removeClass("hidden");
    }
    else {
        $("#Frequency_hr").addClass("hidden");
    }

    // clear the existing entries
    $('#relaychannelconfigurationtable tbody').empty();

    // add as many rows as we need
    for (let CurrentRowId = 0; CurrentRowId < ChannelConfigs.length; CurrentRowId++) {
        // console.log("CurrentRowId = " + CurrentRowId);

        let ChanIdPattern = '<td id="chanId_' + (CurrentRowId) + '">a</td>';
        let EnabledPattern = '<td><input type="checkbox" id="Enabled_' + (CurrentRowId) + '"></td>';
        let EnabledHttpPattern = '<td><input type="checkbox" id="EnabledHttp_' + (CurrentRowId) + '"></td>';
        let InvertedPattern = '<td><input type="checkbox" id="Inverted_' + (CurrentRowId) + '"></td>';
        let PwmPattern = '<td><input type="checkbox" id="Pwm_' + (CurrentRowId) + '"></td>';
        let gpioPattern = '<td><input type="number"   id="gpioId_' + (CurrentRowId) + '"step="1" min="-1" max="34"  value="34"  class="form-control is-valid"></td>';
        let threshholdPattern = '<td><input type="number"   id="threshhold_' + (CurrentRowId) + '"step="1" min="0" max="255" value="300" class="form-control is-valid"></td>';
        let PwmFreqPattern = '';
        if (true === HasPwmFrequency) {
            PwmFreqPattern = '<td><input type="number"   id="Frequency_' + (CurrentRowId) + '"step="1" min="100" max="19000" value="19000" class="form-control is-valid"></td>';
        }

        let rowPattern = '<tr>' + ChanIdPattern + EnabledPattern + EnabledHttpPattern + InvertedPattern + PwmPattern + gpioPattern + threshholdPattern + PwmFreqPattern + '</tr>';
        $('#relaychannelconfigurationtable tr:last').after(rowPattern);

        $('#chanId_'      + CurrentRowId).attr('style', $('#chanId_hr').attr('style'));
        $('#Enabled_'     + CurrentRowId).attr('style', $('#Enabled_hr').attr('style'));
        $('#EnabledHttp_' + CurrentRowId).attr('style', $('#EnabledHttp_hr').attr('style'));
        $('#Inverted_'    + CurrentRowId).attr('style', $('#Inverted_hr').attr('style'));
        $('#Pwm_'         + CurrentRowId).attr('style', $('#Pwm_hr').attr('style'));
        $('#gpioId_'      + CurrentRowId).attr('style', $('#gpioId_hr').attr('style'));
        $('#threshhold_'  + CurrentRowId).attr('style', $('#threshhold_hr').attr('style'));
        if (true === HasPwmFrequency) {
            $('#Frequency_' + CurrentRowId).attr('style', $('#Frequency_hr').attr('style'));
        }
    }

    // populate config
    $.each(ChannelConfigs, function (i, CurrentChannelConfig) {
        let currentChannelRowId = CurrentChannelConfig.id;
        // console.log("Populate Config Current Channel Id = " + CurrentChannelConfig.id);
        // console.log("Populate Config Current gid = " + CurrentChannelConfig.gid);
        $('#chanId_'      + (currentChannelRowId)).html(currentChannelRowId + 1);
        $('#Enabled_'     + (currentChannelRowId)).prop("checked", CurrentChannelConfig.en);
        $('#EnabledHttp_' + (currentChannelRowId)).prop("checked", CurrentChannelConfig.enhttp);
        $('#Inverted_'    + (currentChannelRowId)).prop("checked", CurrentChannelConfig.inv);
        $('#Pwm_'         + (currentChannelRowId)).prop("checked", CurrentChannelConfig.pwm);
        $('#gpioId_'      + (currentChannelRowId)).val(CurrentChannelConfig.gid);
        $('#threshhold_'  + (currentChannelRowId)).val(CurrentChannelConfig.trig);
        if (true === HasPwmFrequency) {
            $('#Frequency_' + (currentChannelRowId)).val(CurrentChannelConfig.Frequency);
        }
    });

} // ProcessModeConfigurationDataRelay

function ProcessModeConfigurationDataGrinch(GrinchConfig)
{
    // console.log("ProcessModeConfigurationDataGrinch");
    // console.debug("GrinchConfig: " + JSON.stringify(GrinchConfig));

    $('#grinch #controller_count' ).val(GrinchConfig.count);
    $('#grinch #cs_pin' ).val(GrinchConfig.dataspi.cs_pin);
    $('#grinch #data_pin' ).val(GrinchConfig.dataspi.data_pin);
    $('#grinch #clock_pin' ).val(GrinchConfig.dataspi.clock_pin);

} // ProcessModeConfigurationDataGrinch

function ProcessModeConfigurationDataServoPCA9685(ServoConfig) {
    // console.log("Servochannelconfigurationtable.rows.length = " + $('#servo_pca9685channelconfigurationtable tr').length);

    let ChannelConfigs = ServoConfig.channels;

    // add as many rows as we need
    for (let CurrentRowId = 1; CurrentRowId <= ChannelConfigs.length; CurrentRowId++) {
        // console.log("CurrentRowId = " + CurrentRowId);
        let ChanIdPattern = '<td                        id="ServoChanId_' + (CurrentRowId) + '">a</td>';
        let EnabledPattern = '<td><input type="checkbox" id="ServoEnabled_' + (CurrentRowId) + '"></td>';
        let MinLevelPattern = '<td><input type="number"   id="ServoMinLevel_' + (CurrentRowId) + '"step="1" min="10" max="4095"  value="0"  class="form-control is-valid"></td>';
        let MaxLevelPattern = '<td><input type="number"   id="ServoMaxLevel_' + (CurrentRowId) + '"step="1" min="10" max="4095"  value="0"  class="form-control is-valid"></td>';
        let RestingPattern = '<td><input type="number"   id="ServoHomeValue_' + (CurrentRowId) + '"step="1" min="0"  max="255"   value="0"  class="form-control is-valid"></td>';
        let DataType = '<td><select class="form-control is-valid" id="ServoDataType_' + (CurrentRowId) + '" title="Effect to generate"></select></td>';

        let rowPattern = '<tr>' + ChanIdPattern + EnabledPattern + MinLevelPattern + MaxLevelPattern + DataType + RestingPattern + '</tr>';

        $('#servo_pca9685channelconfigurationtable tr:last').after(rowPattern);

        $('#ServoChanId_'      + CurrentRowId).attr('style', $('#ServoChanId_hr').attr('style'));
        $('#ServoEnabled_'     + CurrentRowId).attr('style', $('#ServoEnabled_hr').attr('style'));
        $('#ServoMinLevel_'    + CurrentRowId).attr('style', $('#ServoMinLevel_hr').attr('style'));
        $('#ServoMaxLevel_'    + CurrentRowId).attr('style', $('#ServoMaxLevel_hr').attr('style'));
        $('#ServoHomeValue_'   + CurrentRowId).attr('style', $('#ServoHomeValue_hr').attr('style'));
        $('#ServoDataType_'    + CurrentRowId).attr('style', $('#ServoDataType_hr').attr('style'));
    }

    $.each(ChannelConfigs, function (i, CurrentChannelConfig) {
        // console.log("Current Channel Id = " + CurrentChannelConfig.id);
        let currentChannelRowId = CurrentChannelConfig.id + 1;
        $('#ServoChanId_' + (currentChannelRowId)).html(currentChannelRowId);
        $('#ServoEnabled_' + (currentChannelRowId)).prop("checked", CurrentChannelConfig.en);
        $('#ServoMinLevel_' + (currentChannelRowId)).val(CurrentChannelConfig.Min);
        $('#ServoMaxLevel_' + (currentChannelRowId)).val(CurrentChannelConfig.Max);
        $('#ServoHomeValue_' + (currentChannelRowId)).val(CurrentChannelConfig.hv);

        let jqSelector = "#ServoDataType_" + (currentChannelRowId);

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
            (CurrentChannelConfig.b16 << 2));
    });

} // ProcessModeConfigurationDataServoPCA9685

function ProcessInputConfig() {
    $('#ecb_enable').prop("checked", Input_Config.ecb.enabled);
    $('#ecb_gpioid').val(Input_Config.ecb.id);
    $('#ecb_polarity').val(Input_Config.ecb.polarity);
    $('#ecb_longPress').val(Input_Config.ecb.long);

    if ({}.hasOwnProperty.call(Input_Config.channels[1][2], "tsensortopic")) {
        $("#temperatureSensor").removeClass("hidden");
    }
    else {
        $("#temperatureSensor").addClass("hidden");
    }

} // ProcessInputConfig

function ProcessModeConfigurationData(channelId, ChannelType, JsonConfig) {
    // console.debug("ProcessModeConfigurationData: Start");
    // console.debug("channelId: " + channelId);
    // console.debug("ChannelType: " + ChannelType);
    // console.debug("JsonConfig: " + JSON.stringify(JsonConfig));
    // determine the type of in/output that has been selected and populate the form
    let TypeOfChannelId = parseInt($('#' + ChannelType + channelId + " option:selected").val(), 10);
    let channelConfigSet = JsonConfig.channels[channelId];
    // console.debug("ChannelType: " + ChannelType);
    // console.debug("channelId: " + channelId);

    if (isNaN(TypeOfChannelId)) {
        // use the value we got from the controller
        TypeOfChannelId = channelConfigSet.type;
    }
    let channelConfig = channelConfigSet[TypeOfChannelId];
    let ChannelTypeName = channelConfig.type.toLowerCase();
    ChannelTypeName = ChannelTypeName.replace(".", "_");
    ChannelTypeName = ChannelTypeName.replace(" ", "_");
    // console.debug("ChannelTypeName: " + ChannelTypeName);

    let elementids = [];
    let modeControlName = '#' + ChannelType + 'mode' + channelId;
    // console.debug("modeControlName: " + modeControlName);

    // modify page title
    //TODO: Dirty hack to clean-up input names
    if ((ChannelType !== 'input') && ('disabled' !== ChannelTypeName))
    {
        let ModeDisplayName = GenerateInputOutputControlLabel(ChannelType, channelId) + " - " + $(modeControlName + ' #Title')[0].innerHTML;
        // console.debug("ModeDisplayName: " + ModeDisplayName);
        $(modeControlName + ' #Title')[0].innerHTML = ModeDisplayName;
    }

    elementids = $(modeControlName + ' *[id]').filter(":input").map(function () {
        return $(this).attr('id');
    }).get();

    elementids.forEach(function (elementid) {
        let SelectedElement = modeControlName + ' #' + elementid;
        if ($(SelectedElement).is(':checkbox')) {
            $(SelectedElement).prop('checked', channelConfig[elementid]);
        }
        else {
            $(SelectedElement).val(channelConfig[elementid]);
        }
    });

    // by default, do not show the ECB config data
    $('#ecb').addClass("hidden");

    if ("disabled" === ChannelTypeName)
    {
        // console.debug("Process Disabled");
        $(modeControlName).empty();
        $('#refresh').html('-');
    }
    else if ("fpp_remote" === ChannelTypeName)
    {
        $('#ecb').removeClass("hidden");
        if (null !== Fseq_File_List)
        {
            ProcessModeConfigurationDatafppremote(channelConfig);
        }
    }

    else if ("effects" === ChannelTypeName) {
        $('#ecb').removeClass("hidden");
        ProcessModeConfigurationDataEffects(channelConfig);
    }

    else if ("relay" === ChannelTypeName) {
        // console.debug("ProcessModeConfigurationData: relay");
        ProcessModeConfigurationDataRelay(channelConfig);
    }

    else if ("servo_pca9685" === ChannelTypeName) {
        // console.debug("ProcessModeConfigurationData: servo");
        ProcessModeConfigurationDataServoPCA9685(channelConfig);
    }

    else if ("grinch" === ChannelTypeName) {
        // console.debug("ProcessModeConfigurationData: grinch");
        ProcessModeConfigurationDataGrinch(channelConfig);
    }

    UpdateAdvancedOptionsMode();
    UpdateChannelCounts();

    // console.debug("ProcessModeConfigurationData: End");

} // ProcessModeConfigurationData

function ProcessReceivedJsonConfigMessage(JsonConfigData) {
    // console.debug("ProcessReceivedJsonConfigMessage: Start");

    // is this an output config?
    if ({}.hasOwnProperty.call(JsonConfigData, "output_config"))
    {
        // save the config for later use.
        Output_Config = JsonConfigData.output_config;
        // console.debug("Got Output Config: " + JSON.stringify(Output_Config) );
        CreateOptionsFromConfig("output", Output_Config);
    }

    // is this an input config?
    else if ({}.hasOwnProperty.call(JsonConfigData, "input_config"))
    {
        // save the config for later use.
        Input_Config = JsonConfigData.input_config;
        CreateOptionsFromConfig("input", Input_Config);
    }

    // is this a device config?
    else if ({}.hasOwnProperty.call(JsonConfigData, "system")) {
        System_Config = JsonConfigData.system;
        // console.debug("Got System Config: " + JSON.stringify(System_Config) );

        updateFromJSON(System_Config);
        $('#ftpusername').val(System_Config.device.user);
        $('#ftppassword').val(System_Config.device.password);
        $('#ftp_enable').prop("checked", System_Config.device.enabled);

        if ({}.hasOwnProperty.call(System_Config.network, 'eth')) {
            $('#pg_network #network #eth').removeClass("hidden")
        }
        else {
            $('#pg_network #network #eth').addClass("hidden")
        }

        if ({}.hasOwnProperty.call(System_Config, 'sensor')) {
            $('#TemperatureSensorGrp').removeClass("hidden");
            $('#TemperatureSensorUnits').val(System_Config.sensor.units);
        }
        else {
            $('#TemperatureSensorGrp').addClass("hidden");
        }
    }

    // is this a file list?
    else if ({}.hasOwnProperty.call(JsonConfigData, "files")) {
        ProcessGetFileListResponse(JsonConfigData);
    }

    // is this an admin msg?
    else if ({}.hasOwnProperty.call(JsonConfigData, "admin")) {
        ProcessReceivedJsonAdminMessage(JsonConfigData);
    }

    // is this an ACK response?
    else if ({}.hasOwnProperty.call(JsonConfigData, "OK")) {
        // console.debug("Received Acknowledgement to config set command.")
    }


    else {
        console.error("unknown configuration record type has been ignored.")
    }

    // console.debug("ProcessReceivedJsonConfigMessage: Done");

} // ProcessReceivedJsonConfigMessage

// Builds jQuery selectors from JSON data and updates the web interface
function updateFromJSON(obj) {
    for (let k in obj) {
        selector.push('#' + k);
        if (typeof obj[k] === 'object' && obj[k] !== null) {
            updateFromJSON(obj[k]);
        }
        else {
            let jqSelector = selector.join(' ');
            if (typeof obj[k] === 'boolean') {
                $(jqSelector).prop('checked', obj[k]);
            }
            else {
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
} // updateFromJSON

function GenerateInputOutputControlLabel(OptionListName, DisplayedChannelId) {
    let Id = parseInt(DisplayedChannelId) + 1;
    let NewName = '';
    //TODO: Dirty Hack to clean-up Input lables
    if (OptionListName === `input`) {
        NewName = (Id === 1) ? 'Primary Input' : 'Secondary Input'
    } else {
        NewName = OptionListName.charAt(0).toUpperCase() + OptionListName.slice(1) + " " + Id;
    }
    // console.log(`IO Label: ${NewName}`)
    return NewName;

} // GenerateInputOutputControlLabel

function loadContentSync(url, targetElement, OptionListName, DisplayedChannelId)
{
    $.ajax(
    {
        url: url,
        async: false,
        success: function(data)
        {
            // console.info("Process Received Mode Config file.");
            $(targetElement).html(data);
            if ("input" === OptionListName)
            {
                ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Input_Config);
                ProcessInputConfig();
            }
            else if ("output" === OptionListName)
            {
                ProcessModeConfigurationData(DisplayedChannelId, OptionListName, Output_Config);

                // Trigger refresh update for outputs
                $('#fg_output_mode input').trigger('change');
            }
        },
        error: function()
        {
            console.error('Failed to load: ' + url);
        }
    });
} // loadContentSync

function LoadDeviceSetupSelectedOption(OptionListName, DisplayedChannelId)
{
    // console.debug("OptionListName: " + OptionListName);
    // console.debug("DisplayedChannelId: " + DisplayedChannelId);

    let HtmlLoadFileName = $('#' + OptionListName + DisplayedChannelId + ' option:selected').text().toLowerCase();
    // console.debug("Base HtmlLoadFileName: " + HtmlLoadFileName);
    HtmlLoadFileName = HtmlLoadFileName.replace(".", "_");
    HtmlLoadFileName = HtmlLoadFileName.replace(" ", "_");
    HtmlLoadFileName = HtmlLoadFileName + ".html";
    // console.debug("Adjusted HtmlLoadFileName: " + HtmlLoadFileName);

    //TODO: Detect modules that don't require configuration - DDP, Alexa, ?
    // try to load the field definition file for this channel type
    loadContentSync(HtmlLoadFileName, '#' + OptionListName + 'mode' + DisplayedChannelId, OptionListName, DisplayedChannelId);

} // LoadDeviceSetupSelectedOption

function CreateOptionsFromConfig(OptionListName, Config) {
    // console.debug("CreateOptionsFromConfig");

    // Set selection column width based on arch which equates to number of outputs for now
    let col = (AdminInfo.arch === 'ESP8266') ? '4' : '2';
    let Channels = Config.channels;

    if ("input" === OptionListName) {
        $('#ecpin').val(Config.ecpin);
    }

    // for each field we need to populate (input vs output)
    Object.keys(Channels).forEach(function (ChannelId) {
        // OptionListName is 'input' or 'output'
        // console.debug("ChannelId: " + ChannelId);
        let CurrentChannel = Channels[ChannelId];

        // does the selection box we need already exist?
        if (!$('#' + OptionListName + 'mode' + ChannelId).length) {
            // console.log(`OptionListName: ${OptionListName}`)
            // create the selection box
            $(`#fg_${OptionListName}`).append(`<label class="control-label col-sm-2" for="${OptionListName}${ChannelId}">${GenerateInputOutputControlLabel(OptionListName, ChannelId)}</label>`);
            $(`#fg_${OptionListName}`).append(`<div class="col-sm-${col}"><select class="form-control wsopt" id="${OptionListName}${ChannelId}"></select></div>`);
            $(`#fg_${OptionListName}_mode`).append(`<fieldset id="${OptionListName}mode${ChannelId}"></fieldset>`);
        }

        let jqSelector = "#" + OptionListName + ChannelId;

        // remove the existing options
        $(jqSelector).empty();

        // for each Channel type in the list
        Object.keys(CurrentChannel).forEach(function (SelectionTypeId) {
            // console.debug("SelectionId: " + SelectionTypeId);
            if ("type" === SelectionTypeId) {
                // console.debug("Set the selector type to: " + CurrentChannel.type);
                $(jqSelector).val(CurrentChannel.type);
                LoadDeviceSetupSelectedOption(OptionListName, ChannelId);
                $(jqSelector).on("change", function () {
                    // console.debug("Set the selector type to: " + CurrentChannel.type);
                    LoadDeviceSetupSelectedOption(OptionListName, ChannelId);
                });
            }
            else {
                let CurrentSection = CurrentChannel[SelectionTypeId];
                // console.debug("Add '" + CurrentSection.type + "' to selector");
                $(jqSelector).append('<option value="' + SelectionTypeId + '">' + CurrentSection.type + '</option>');
            }
        }); // end for each selection type
    }); // end for each channel
} // CreateOptionsFromConfig

// Builds JSON config submission for "WiFi" tab
function ExtractNetworkWiFiConfigFromHtmlPage() {
    let wifi = System_Config.network.wifi;
    wifi.ssid = $('#network #wifi #ssid').val();
    wifi.passphrase = $('#network #wifi #passphrase').val();
    wifi.sta_timeout = parseInt($('#network #wifi #sta_timeout').val());
    wifi.ip = $('#network #wifi #ip').val();
    wifi.netmask = $('#network #wifi #netmask').val();
    wifi.gateway = $('#network #wifi #gateway').val();
    wifi.dnsp = $('#network #wifi #dnsp').val();
    wifi.dnss = $('#network #wifi #dnss').val();
    wifi.dhcp = $('#network #wifi #dhcp').prop('checked');
    wifi.ap_ssid = $('#network #wifi #ap_ssid').val();
    wifi.ap_passphrase = $('#network #wifi #ap_passphrase').val();
    wifi.ap_channel = parseInt($('#network #wifi #ap_channel').val());
    wifi.ap_fallback = $('#network #wifi #ap_fallback').prop('checked');
    wifi.ap_reboot = $('#network #wifi #ap_reboot').prop('checked');
    wifi.ap_timeout = parseInt($('#network #wifi #ap_timeout').val());
    wifi.StayInApMode = $('#network #wifi #StayInApMode').prop('checked');

} // ExtractNetworkWiFiConfigFromHtmlPage

function ExtractNetworkEthernetConfigFromHtmlPage() {
    if ({}.hasOwnProperty.call(System_Config.network, "eth")) {
        System_Config.network.weus = $('#network #eth #weus').prop('checked');

        System_Config.network.eth.ip = $('#network #eth #ip').val();
        System_Config.network.eth.netmask = $('#network #eth #netmask').val();
        System_Config.network.eth.gateway = $('#network #eth #gateway').val();
        System_Config.network.eth.dnsp = $('#network #eth #dnsp').val();
        System_Config.network.eth.dnss = $('#network #eth #dnss').val();
        System_Config.network.eth.dhcp = $('#network #eth #dhcp').prop('checked');
        System_Config.network.eth.type = parseInt($('#network #eth #type option:selected').val(), 10);
        System_Config.network.eth.addr = parseInt($('#network #eth #addr').val());
        System_Config.network.eth.power_pin = parseInt($('#network #eth #power_pin').val());
        System_Config.network.eth.mode = parseInt($('#network #eth #mode option:selected').val());
        System_Config.network.eth.mdc_pin = parseInt($('#network #eth #mdc_pin').val());
        System_Config.network.eth.mdio_pin = parseInt($('#network #eth #mdio_pin').val());
        System_Config.network.eth.activevalue = (parseInt($('#network #eth #activevalue option:selected').val(), 10) === 1);
        System_Config.network.eth.activedelay = parseInt($('#network #eth #activedelay').val());
    }

} // ExtractNetworkEthernetConfigFromHtmlPage

// Builds JSON config submission for "Network" tab
function ExtractNetworkConfigFromHtmlPage() {
    System_Config.network.hostname = $('#hostname').val();

    ExtractNetworkWiFiConfigFromHtmlPage();
    ExtractNetworkEthernetConfigFromHtmlPage();

} // ExtractNetworkConfigFromHtmlPage

// Builds JSON config submission for "WiFi" tab
function submitNetworkConfig() {
    System_Config.device.id = $('#config #device #id').val();
    System_Config.device.blanktime = parseInt($('#config #device #blanktime').val());
    System_Config.device.miso_pin = parseInt($('#config #device #miso_pin').val());
    System_Config.device.mosi_pin = parseInt($('#config #device #mosi_pin').val());
    System_Config.device.clock_pin = parseInt($('#config #device #clock_pin').val());
    System_Config.device.cs_pin = parseInt($('#config #device #cs_pin').val());
    System_Config.device.sdspeed = parseInt($('#config #device #sdspeed').val());
    System_Config.device.user = $('#ftpusername').val();
    System_Config.device.password = $('#ftppassword').val();
    System_Config.device.enabled = $('#ftp_enable').prop('checked');

    if ({}.hasOwnProperty.call(System_Config, 'sensor')) {
        System_Config.sensor.units = parseInt($('#TemperatureSensorUnits').val());
    }

    ExtractNetworkConfigFromHtmlPage();

    SendAllConfigFilesToServer();
} // submitNetworkConfig

function ExtractChannelConfigFromHtmlPage(JsonConfig, SectionName) {
    // for each option channel:
    jQuery.each(JsonConfig, function (DisplayedChannelId, CurrentChannelConfigurationData) {
        let elementids = [];
        let modeControlName = '#' + SectionName + 'mode' + DisplayedChannelId;
        elementids = $(modeControlName + ' *[id]').filter(":input").map(function ()
        {
            return $(this).attr('id');
        }).get();

        // console.debug("modeControlName: " + modeControlName);

        let ChannelType = parseInt($("#" + SectionName + DisplayedChannelId + " option:selected").val(), 10);
        let ChannelConfig = CurrentChannelConfigurationData[ChannelType];

        // tell the ESP what type of channel it should be using
        CurrentChannelConfigurationData.type = ChannelType;

        if ((ChannelConfig.type === "Relay") && ($("#relaychannelconfigurationtable").length)) {
            ChannelConfig.updateinterval = parseInt($('#updateinterval').val(), 10);
            $.each(ChannelConfig.channels, function (i, CurrentChannelConfig) {
                // console.debug("Current Channel Id = " + CurrentChannelConfig.id);
                let currentChannelRowId     = CurrentChannelConfig.id;
                CurrentChannelConfig.en     = $('#Enabled_' + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.enhttp = $('#EnabledHttp_' + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.inv    = $('#Inverted_' + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.pwm    = $('#Pwm_' + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.gid    = parseInt($('#gpioId_' + (currentChannelRowId)).val(), 10);
                CurrentChannelConfig.trig   = parseInt($('#threshhold_' + (currentChannelRowId)).val(), 10);

                if ({}.hasOwnProperty.call(ChannelConfig, "Frequency")) {
                    CurrentChannelConfig.Frequency = parseInt($('#Frequency_' + (currentChannelRowId)).val(), 10);
                }
            });
        }
        else if ((ChannelConfig.type === "Servo PCA9685") && ($("#servo_pca9685channelconfigurationtable").length)) {
            ChannelConfig.updateinterval = parseInt($('#updateinterval').val(), 10);
            $.each(ChannelConfig.channels, function (i, CurrentChannelConfig) {
                // console.debug("Current Channel Id = " + CurrentChannelConfig.id);
                let currentChannelRowId = CurrentChannelConfig.id + 1;
                CurrentChannelConfig.en = $('#ServoEnabled_' + (currentChannelRowId)).prop("checked");
                CurrentChannelConfig.Min = parseInt($('#ServoMinLevel_' + (currentChannelRowId)).val(), 10);
                CurrentChannelConfig.Max = parseInt($('#ServoMaxLevel_' + (currentChannelRowId)).val(), 10);
                CurrentChannelConfig.hv = parseInt($('#ServoHomeValue_' + (currentChannelRowId)).val(), 10);
                let ServoDataType = parseInt($('#ServoDataType_' + (currentChannelRowId)).val(), 10);

                CurrentChannelConfig.rev = (ServoDataType & 0x01) ? true : false;
                CurrentChannelConfig.sca = (ServoDataType & 0x02) ? true : false;
                CurrentChannelConfig.b16 = (ServoDataType & 0x04) ? true : false;
            });
        }
        else if (ChannelConfig.type === "Effects") {
            // transitions need extra processing
            ExtractConfigFromHtmlPages(elementids, modeControlName, ChannelConfig);

            // the auto export adds the add color button to the structure. Remove it.
            delete ChannelConfig["AddTransitionBtn"];
            delete ChannelConfig["AddMarqueeGroupBtn"];

            // build a new transitions array
            const transitions = [];
            transitions.length = $('#TransitionColorTable tbody tr').length - 1;
            let elementId = 0; // row counter into the json array

            $('#TransitionColorTable tbody tr').each(function () {
                let CurRow = $(this)[0];
                let RowId = $(CurRow).attr("RowId");

                if (undefined !== RowId) {
                    // console.debug("RowId = " + RowId);

                    let DeleteButtonName = 'transitionDelete_' + RowId;
                    let elementName = 'transitionColor_' + RowId;
                    // console.debug("DeleteButtonName = " + DeleteButtonName);
                    // console.debug("elementName = " + elementName);

                    // the auto export adds the delete and raw color data to the structure. Remove it.
                    delete ChannelConfig[DeleteButtonName];
                    delete ChannelConfig[elementName];

                    let HexValue = $('#' + elementName).val();

                    // console.debug("HexValue = " + HexValue);
                    // console.debug("r = " + hexToRgb(HexValue).r);
                    // console.debug("g = " + hexToRgb(HexValue).g);
                    // console.debug("b = " + hexToRgb(HexValue).b);
                    transitions[elementId] = {};
                    let transition = transitions[elementId];
                    transition.r = hexToRgb(HexValue).r;
                    transition.g = hexToRgb(HexValue).g;
                    transition.b = hexToRgb(HexValue).b;
                    elementId++;
                }
            });

            ChannelConfig.transitions = transitions;

            // build a new MarqueeGroup array
            const MarqueeGroups = [];
            MarqueeGroups.length = $('#MarqueeGroupTable tbody tr').length - 1;
            elementId = 0; // row counter into the json array

            delete ChannelConfig["MarqueeGroups"];

            $('#MarqueeGroupTable tbody tr').each(function () {
                let CurRow = $(this)[0];
                let RowId = $(CurRow).attr("RowId");

                if (undefined !== RowId) {
                    // console.debug("RowId = " + RowId);
                    let DeleteButtonName = 'MarqueeGroupDelete_' + RowId;
                    let ColorElementName = 'MarqueeGroupColor_' + RowId;
                    // console.debug("DeleteButtonName = " + DeleteButtonName);
                    // console.debug("elementName = " + elementName);

                    // the auto export adds the delete and table data to the structure. Remove it.
                    delete ChannelConfig[DeleteButtonName];
                    delete ChannelConfig['MarqueeGroupIntensity_' + (RowId)];
                    delete ChannelConfig['MarqueeGroupIntensityEnd_' + (RowId)];
                    delete ChannelConfig['MarqueeGroupCount_' + (RowId)];
                    delete ChannelConfig['MarqueeGroupColor_' + (RowId)];

                    let HexValue = $('#' + ColorElementName).val();

                    // console.debug("HexValue = " + HexValue);
                    // console.debug("r = " + hexToRgb(HexValue).r);
                    // console.debug("g = " + hexToRgb(HexValue).g);
                    // console.debug("b = " + hexToRgb(HexValue).b);

                    MarqueeGroups[elementId] = {};
                    let MarqueeGroup = MarqueeGroups[elementId];

                    MarqueeGroup.brightness = parseFloat($('#MarqueeGroupIntensity_' + (RowId)).val());
                    MarqueeGroup.brightnessEnd = parseFloat($('#MarqueeGroupIntensityEnd_' + (RowId)).val());
                    MarqueeGroup.pixel_count = parseInt($('#MarqueeGroupCount_' + (RowId)).val());

                    MarqueeGroup.color = {};
                    MarqueeGroup.color.r = hexToRgb(HexValue).r;
                    MarqueeGroup.color.g = hexToRgb(HexValue).g;
                    MarqueeGroup.color.b = hexToRgb(HexValue).b;
                    elementId++;
                }
            });

            ChannelConfig.MarqueeGroups = MarqueeGroups;
        }
        else if(ChannelConfig.type === "Grinch")
        {
            ChannelConfig.count             = parseInt($('#grinch #controller_count' ).val());
            ChannelConfig.dataspi.cs_pin    = parseInt($('#grinch #cs_pin' ).val());
            ChannelConfig.dataspi.clock_pin = parseInt($('#grinch #clock_pin' ).val());
            ChannelConfig.dataspi.data_pin  = parseInt($('#grinch #data_pin' ).val());
        }
        else
        {
            ExtractConfigFromHtmlPages(elementids, modeControlName, ChannelConfig);
        }
    });
} // ExtractChannelConfigFromHtmlPage

function hexToRgb(hex) {
    // console.debug("hex: " + hex);
    while (hex.length < 7) { hex = hex.replace("#", "#0"); }
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
    } : null;
} // hexToRgb

function ExtractConfigFromHtmlPages(elementids, modeControlName, ChannelConfig)
{
    // console.debug("Preping config to send to ESP");
    // console.debug("num elementids: " + elementids.length);

    elementids.forEach(function (elementid)
    {
        let SelectedElement = modeControlName + ' #' + elementid;
        // console.debug("Element ID: " + $(SelectedElement).id)

        if ($(SelectedElement).is(':checkbox'))
        {
            ChannelConfig[elementid] = $(SelectedElement).prop('checked');
        }
        else if ($(SelectedElement).attr('type') === 'number')
        {
            // console.debug("Found Number: " + $(SelectedElement).id)
            ChannelConfig[elementid] = parseInt($(SelectedElement).val());
        }
        else
        {
            ChannelConfig[elementid] = $(SelectedElement).val();
        }
    }); // end for each channel
} // ExtractConfigFromHtmlPages

function ValidateConfigFields(ElementList) {
    // return true if errors were found
    let response = false;

    for (let ChildElementId = 0;
        ChildElementId < ElementList.length;
        ChildElementId++) {
        let ChildElement = ElementList[ChildElementId];
        // let ChildType = ChildElement.type;

        if ((ChildElement.validity.valid !== undefined) && (!$(ChildElement).hasClass('hidden'))) {
            // console.debug("ChildElement.validity.valid: " + ChildElement.validity.valid);
            if (false === ChildElement.validity.valid) {
                // console.debug("          Element: " + ChildElement.id);
                // console.debug("   ChildElementId: " + ChildElementId);
                // console.debug("ChildElement Type: " + ChildType);
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

    Input_Config.ecb.enabled = $('#ecb_enable').is(':checked');
    Input_Config.ecb.id = parseInt($('#ecb_gpioid').val());
    Input_Config.ecb.polarity = $("#ecb_polarity").val();
    Input_Config.ecb.long = parseInt($("#ecb_longPress").val());

    if ({}.hasOwnProperty.call(System_Config, 'sensor')) {
        System_Config.sensor.units = parseInt($('#TemperatureSensorUnits').val());
    }

    ExtractChannelConfigFromHtmlPage(Output_Config.channels, "output");
    submitNetworkConfig();

} // submitDeviceConfig

function int2ip(num) {
    let d = num % 256;
    for (let i = 3; i > 0; i--) {
        num = Math.floor(num / 256);
        d = d + '.' + num % 256;
    }
    return d;
} // int2ip

// Ping every 4sec if there is no other traffic
function MonitorServerConnection()
{
    // console.debug("MonitorServerConnection");
    if(null === ServerTransactionTimer)
    {
        // console.info("MonitorServerConnection: Expired");
        // console.info("PreviousMonitorTransactionRequestInProgressId: " + PreviousMonitorTransactionRequestInProgressId);
        // console.info("        MonitorTransactionRequestInProgressId: " + MonitorTransactionRequestInProgressId);
        // console.info("            FailedToCompleteServerTransaction: " + FailedToCompleteServerTransaction);

        if(PreviousMonitorTransactionRequestInProgressId === MonitorTransactionRequestInProgressId)
        {
            SendCommand('XP');
        }
        PreviousMonitorTransactionRequestInProgressId = MonitorTransactionRequestInProgressId;

        $('#wserror').modal((FailedToCompleteServerTransaction > 1) ? "show" : "hide");

        ServerTransactionTimer = setTimeout(function ()
        {
            clearTimeout(ServerTransactionTimer);
            ServerTransactionTimer = null;

            MonitorServerConnection();
        }, 4000);
    }
} // MonitorServerConnection

function drawStream(streamData) {
    let cols = parseInt($('#v_columns').val());
    let size = Math.floor((canvas.width - 20) / cols);
    let maxDisplay = 0;

    if ($("#diag #viewStyle option:selected").val() === "rgb") {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)) * 3);
        for (let i = 0; i < maxDisplay; i += 3) {
            ctx.fillStyle = 'rgb(' + streamData[i + 0] + ',' + streamData[i + 1] + ',' + streamData[i + 2] + ')';
            let col = (i / 3) % cols;
            let row = Math.floor((i / 3) / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 1, size - 1);
        }
    }
    else if ($("#diag #viewStyle option:selected").val() === "rgbw") {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)) * 4);
        for (let i = 0; i < maxDisplay; i += 4) {
            let WhiteLevel = streamData[i + 3];
            ctx.fillStyle = 'rgb(' + Math.max(streamData[i + 0], WhiteLevel) + ',' + Math.max(streamData[i + 1], WhiteLevel) + ',' + Math.max(streamData[i + 2], WhiteLevel) + ')';
            let col = (i / 4) % cols;
            let row = Math.floor((i / 4) / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 1, size - 1);
        }
    }
    else {
        maxDisplay = Math.min(streamData.length, (cols * Math.floor((canvas.height - 30) / size)));
        for (let i = 0; i < maxDisplay; i++) {
            ctx.fillStyle = 'rgb(' + streamData[i] + ',' + streamData[i] + ',' + streamData[i] + ')';
            let col = (i) % cols;
            let row = Math.floor(i / cols);
            ctx.fillRect(10 + (col * size), 10 + (row * size), size - 2, size - 2);
        }
    }
    if (streamData.length > maxDisplay) {
        ctx.fillStyle = 'rgb(204,0,0)';
        ctx.fillRect(0, canvas.height - 25, canvas.width, 25);
        ctx.fillStyle = 'rgb(255,255,255)';
        ctx.fillText("Increase number of columns to show all data", (canvas.width / 2), canvas.height - 5);
    }
} // drawStream

function clearStream() {
    if (typeof ctx !== 'undefined') {
        ctx.clearRect(0, 0, canvas.width, canvas.height);
    }
} // clearStream

function ProcessReceivedJsonAdminMessage(data)
{
    // console.debug("ProcessReceivedJsonAdminMessage");
    // let ParsedJsonAdmin = JSON.parse(data);
    AdminInfo = data.admin;

    $('#version').text(AdminInfo.version);
    $('#built').text(AdminInfo.built);
    $('#arch').text(AdminInfo.arch);
    $('#usedflashsize').text(AdminInfo.usedflashsize);
    $('#realflashsize').text(AdminInfo.realflashsize);
    $('#flashchipid').text(AdminInfo.flashchipid);
    $('#BoardName').text(AdminInfo.BoardName);

    // Hide elements that are not applicable to our architecture
    if (AdminInfo.arch === "ESP8266") {
        $('.esp32').addClass('hidden');
    } else if (AdminInfo.arch === "ESP32") {
        $('.esp8266').addClass('hidden');
    }

} // ProcessReceivedJsonAdminMessage

function ProcessReceivedJsonStatusMessage(JsonStat) {
    // let JsonStat = JSON.parse(data);
    let Status = JsonStat.status;
    let System = Status.system;
    let Network = System.network;
    let Wifi = Network.wifi;
    let Ethernet = Network.wifi;

    let rssi = Wifi.rssi;
    let quality = rssi + 100;

    if (rssi <= -100) {
        quality = 0;
    }
    else if (rssi >= 0) {
        quality = 100;
    }

    $('#w_connected').text((true === Wifi.connected) ? "Yes" : "No");
    $('#w_hostname').text(Wifi.hostname);
    $('#w_rssi').text(rssi);
    $('#w_quality').text(rssiToPercent[quality]);
    $('#w_ssid').text(Wifi.ssid);
    $('#w_ip').text(Wifi.ip);
    $('#w_subnet').text(Wifi.subnet);
    $('#w_mac').text(Wifi.mac);

    if ({}.hasOwnProperty.call(Network, 'eth')) {
        $('#ethernet_status').removeClass("hidden")
        Ethernet = Network.eth;
        $('#e_connected').text((true === Ethernet.connected) ? "Yes" : "No");
        $('#e_hostname').text(Ethernet.hostname);
        $('#e_ip').text(Ethernet.ip);
        $('#e_subnet').text(Ethernet.subnet);
        $('#e_mac').text(Ethernet.mac);
    }
    else {
        $('#ethernet_status').addClass("hidden")
    }

    // getHeap(data)
    $('#x_freeheap').text(System.freeheap);

    if ({}.hasOwnProperty.call(System, 'HeapDetails'))
    {
        let HeapDetails = System.HeapDetails;
        $('#n804_Free_Max').text(HeapDetails.n804_Free_Max);
        $('#n804_Free_Tot').text(HeapDetails.n804_Free_Tot);
        $('#n80C_Free_Max').text(HeapDetails.n80C_Free_Max);
        $('#n80C_Free_Tot').text(HeapDetails.n80C_Free_Tot);
        $('#n1800_Free_Max').text(HeapDetails.n1800_Free_Max);
        $('#n1800_Free_Tot').text(HeapDetails.n1800_Free_Tot);
    }

    // getUptime
    // uptime is reported in milliseconds
    let date = new Date(System.uptime);
    let str = '';

    //    let hoursPerDay = 24;
    //    let MinutesPerHour = 60;
    //    let secondsPerMinute = 60;
    //    let millisecondsPerSecond = 1000;
    //    let MillisecondsPerDay = millisecondsPerSecond * secondsPerMinute * MinutesPerHour * hoursPerDay; = 86400000

    str += Math.floor(date.getTime() / 86400000) + " days, ";
    str += ("0" + date.getUTCHours()).slice(-2) + ":";
    str += ("0" + date.getUTCMinutes()).slice(-2) + ":";
    str += ("0" + date.getUTCSeconds()).slice(-2);
    $('#x_uptime').text(str);

    date = new Date(1000 * System.currenttime);
    // console.debug("DateMS: " + date.getMilliseconds());
    $('#x_currenttime').text(date.toUTCString());
    let CurrDate = new Date();
    // console.debug("CurrDateMS: " + CurrDate);
    let Delta = Math.abs(CurrDate.getTime() - date.getTime())/1000;
    // console.debug("DeltaS: " + Delta);
    if(Delta > 5)
    {
        SetServerTime();
    }
    if ({}.hasOwnProperty.call(System, 'used')) {
        $('#i_size').removeClass("hidden");
        $('#x_size').removeClass("hidden");
        $('#x_size').text(System.size);

        $('#i_used').removeClass("hidden");
        $('#x_used').removeClass("hidden");
        $('#x_used').text(System.used);
    }
    else {
        $('#i_size').addClass("hidden");
        $('#x_size').addClass("hidden");
        $('#i_used').addClass("hidden");
        $('#x_used').addClass("hidden");
    }

    if ({}.hasOwnProperty.call(System, 'sensor')) {
        $('#i_temperature').removeClass("hidden");
        $('#x_temperature').removeClass("hidden");
        $('#x_temperature').text(System.sensor.reading);
    }
    else {
        $('#i_temperature').addClass("hidden");
        $('#x_temperature').addClass("hidden");
    }

    if (true === System.SDinstalled) {
        $("#li-filemanagement").removeClass("hidden");
    }
    else {
        $("#li-filemanagement").addClass("hidden");
    }

    // getE131Status(data)
    let InputStatus = Status.input[0];
    if ({}.hasOwnProperty.call(InputStatus, 'e131')) {
        $('#E131Status').removeClass("hidden")

        $('#uni_first').text(InputStatus.e131.unifirst);
        $('#uni_last').text(InputStatus.e131.unilast);
        $('#pkts').text(InputStatus.e131.num_packets);
        $('#chanlim').text(InputStatus.e131.unichanlim);
        $('#perr').text(InputStatus.e131.packet_errors);
        $('#clientip').text(int2ip(parseInt(InputStatus.e131.last_clientIP)));
    }
    else {
        $('#E131Status').addClass("hidden")
    }

    if ({}.hasOwnProperty.call(InputStatus, 'Artnet')) {
        $('#ArtnetStatus').removeClass("hidden")

        $('#an_uni_first').text(InputStatus.Artnet.unifirst);
        $('#an_uni_last').text(InputStatus.Artnet.unilast);
        $('#an_pkts').text(InputStatus.Artnet.num_packets);
        $('#an_chanlim').text(InputStatus.Artnet.unichanlim);
        $('#an_perr').text(InputStatus.Artnet.packet_errors);
        $('#an_PollCounter').text(InputStatus.Artnet.PollCounter);
        $('#an_clientip').text(InputStatus.Artnet.last_clientIP);
    }
    else {
        $('#ArtnetStatus').addClass("hidden")
    }

    if ({}.hasOwnProperty.call(InputStatus, 'ddp')) {
        $('#ddpStatus').removeClass("hidden")

        $('#ddppacketsreceived').text(InputStatus.ddp.packetsreceived);
        $('#ddpbytesreceived').text(InputStatus.ddp.bytesreceived);
        $('#ddperrors').text(InputStatus.ddp.errors);
        $('#ddplasterror').text(InputStatus.ddp.lasterror);
    }
    else {
        $('#ddpStatus').addClass("hidden")
    }

    InputStatus = Status.input[1];

    if ({}.hasOwnProperty.call(InputStatus, 'Player')) {
        let PlayerStatus = InputStatus.Player;
        if ({}.hasOwnProperty.call(PlayerStatus, 'FPPDiscovery')) {
            $('#FPPRemoteStatus').removeClass("hidden")

            let FPPDstatus = PlayerStatus.FPPDiscovery

            $('#fppsyncreceived').text(FPPDstatus.SyncCount);
            $('#fppsyncadjustments').text(FPPDstatus.SyncAdjustmentCount);
            $('#fppremoteip').text(FPPDstatus.FppRemoteIp);
            $('#fppplayedfilecount').text(FPPDstatus.PlayedFileCount);

            $('#fppremoteFilePlayerFilename').text(FPPDstatus.current_sequence);
            $('#fppremoteFilePlayerTimeElapsed').text(FPPDstatus.time_elapsed);
            $('#fppremoteFilePlayerTimeRemaining').text(FPPDstatus.time_remaining);
            $('#fppremoteFilePlayerlasterror').text(FPPDstatus.errors);
        }
        else {
            $('#FPPRemoteStatus').addClass("hidden")
        }

        if ({}.hasOwnProperty.call(PlayerStatus, 'File')) {
            $('#LocalFilePlayerStatus').removeClass("hidden");

            let FilePlayerStatus = PlayerStatus.File;
            $('#localFilePlayerFilename').text(FilePlayerStatus.current_sequence);
            $('#localFilePlayerTimeElapsed').text(FilePlayerStatus.time_elapsed);
            $('#localFilePlayerTimeRemaining').text(FilePlayerStatus.time_remaining);
            $('#localFilePlayerPlayedfilecount').text(FilePlayerStatus.count);
            $('#localFilePlayerlasterror').text(FilePlayerStatus.errors);
        }
        else {
            $('#LocalFilePlayerStatus').addClass("hidden");
        }

        if ({}.hasOwnProperty.call(PlayerStatus, 'Effect')) {
            $('#LocalEffectPlayerStatus').removeClass("hidden");

            $('#localFilePlayerEffectName').text(PlayerStatus.Effect.currenteffect);
            $('#localFilePlayerEffectTimeRemaining').text(PlayerStatus.Effect.TimeRemaining);
        }
        else {
            $('#LocalEffectPlayerStatus').addClass("hidden")
        }

        if ({}.hasOwnProperty.call(PlayerStatus, 'Paused')) {
            $('#PausedPlayerStatus').removeClass("hidden");

            $('#PausedTimeRemaining').text(PlayerStatus.Paused.TimeRemaining);
        }
        else {
            $('#PausedPlayerStatus').addClass("hidden")
        }
    }
    else {
        $('#LocalPlayListPlayerStatus').addClass("hidden");
        $('#LocalFilePlayerStatus').addClass("hidden");
        $('#LocalEffectPlayerStatus').addClass("hidden");
        $('#PausedPlayerStatus').addClass("hidden");
        $('#FPPRemoteStatus').addClass("hidden");
    }

    let OutputStatus = Status.output[1];
    if(undefined !== OutputStatus)
    {
        if ({}.hasOwnProperty.call(OutputStatus, 'Relay')) {
            $('#RelayStatus').removeClass("hidden")
    
            OutputStatus.Relay.forEach(function (currentRelay) {
                $('#RelayValue_' + currentRelay.id).text(currentRelay.activevalue);
            });
        }
        else {
            $('#RelayStatus').addClass("hidden")
        }
    }

    // Device Refresh is dynamic
    // #refresh is used in device config tab to reflect what refresh rate should be, not what it currently is
    // $('#refresh').text(Status.output[0].framerefreshrate + " fps");
} // ProcessReceivedJsonStatusMessage

let MsgXferCount = 0;
let AggregateErrorFlag = 0;

function ConfigWaitMessageStart(NumEntries = 1)
{
    // console.debug("ConfigWaitMessageStart");
    // console.debug("MsgXferCount " + MsgXferCount);
    // console.debug("NumEntries " + NumEntries);

    $('#snackbar').modal();

    if(1 === MsgXferCount)
    {
        AggregateErrorFlag = 0;
    }
    MsgXferCount += NumEntries;

} // ConfigWaitMessageStart

function ConfigWaitMessageEnd(ErrorFlag)
{
    // console.debug("ConfigWaitMessageEnd");
    // console.debug("ErrorFlag " + ErrorFlag);
    // console.debug("MsgXferCount " + MsgXferCount);
    MsgXferCount --;
    AggregateErrorFlag += ErrorFlag;

    if (0 === MsgXferCount)
    {
        $('#snackbar').modal('hide');

        if(0 === AggregateErrorFlag)
        {
            alert("Configuration Save Success");
        }
        else
        {
            alert("Configuration Save Failed");
        }
    }

} // ConfigWaitMessageEnd

// Show reboot modal
function showReboot() {
    $("#EfuProgressBar").addClass("hidden");
    $('#update').modal('hide');
    $('#reboot').modal();
    setTimeout(function () {
        if ($('#wifi #dhcp').prop('checked')) {
            window.location.assign("/");
        }
        else {
            window.location.assign("http://" + $('#ip').val());
        }
    }, 5000);
} // showReboot

function SendCommand(command)
{
    // console.debug("SendCommand: " + command);
    MonitorTransactionRequestInProgressId++;
    $.ajaxQueue(
    {
        url: command,
        method: 'POST',
        mode: "cors", // no-cors, *cors, same-origin
        dataType: "json",
        timeout: 20000,
        async: false,
        headers: { 'Content-Type': 'application/json' },
        cache: "reload", // *default, no-cache, reload, force-cache, only-if-cached
        credentials: "same-origin", // include, *same-origin, omit
        redirect: "follow", // manual, *follow, error
        referrerPolicy: "no-referrer", // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin, same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
        success: function (webResponse)
        {
            if("" === webResponse)
            {
                // console.debug("SendCommand: Empty webResponse");
                FailedToCompleteServerTransaction = 0;
            }
            else if(webResponse.headers)
            {
                const isJson = webResponse.headers.get('content-type')?.includes('application/json');
                const data = isJson && webResponse.json();

                // console.debug("SendCommand:webResponse.status: " + webResponse.status);
                // console.debug("SendCommand:webResponse.ok: " + webResponse.ok);
                // check for error response
                if (!webResponse.ok)
                {
                    // get error message from body or default to response status
                    const error = (data && data.message) || webResponse.status;
                    console.error("SendCommand: Error: " + Promise.reject(error));
                    FailedToCompleteServerTransaction ++;
                }
                else
                {
                    // console.debug("SendCommand: Transaction complete");
                    FailedToCompleteServerTransaction = 0;
                }
            }
            else
            {
                // console.debug("SendCommand: No headers in webResponse");
                FailedToCompleteServerTransaction = 0;
            }
        },
        error: function (jqXHR, textStatus, errorThrown)
        {
            FailedToCompleteServerTransaction ++;
            console.error('SendCommand: Error: ', textStatus);
            console.error('SendCommand: errorThrown: ', errorThrown);
        }
    });
    // console.debug("End SendCommand: " + command);
} // SendCommand

// Queue reboot
function reboot() {
    showReboot();
    SendCommand('X6');
} // reboot

// Reset config
$('#confirm-reset .btn-ok').on("click", (function () {
    showReboot();
    SendCommand('X7');
}));
