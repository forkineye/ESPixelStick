var mode = 'null';

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
        
        // Collapse the menu on smaller screens
        $('#navbar').removeClass('in').attr('aria-expanded', 'false');
        $('.navbar-toggle').attr('aria-expanded', 'false');

        $('#wserror').on('hidden.bs.modal', function() {
            location.reload();
        });

        // Firmware selection and upload
        $('#efu').change(function () {
            $('#updatefw').submit();
            $('#update').modal({backdrop: 'static', keyboard: false});
        });

        /* Test mode toggles */
        $('#tmode').change(function() {
            $('.tdiv').addClass('hidden');
            $('#'+$('select[name=tmode]').val()).removeClass('hidden');
        });

        /* Color Picker */
        $('.color').colorPicker({
            buildCallback: function($elm) {
                var colorInstance = this.color;
                var colorPicker = this;

                $elm.append('<div class="cp-memory">' +
                    '<div style="background-color: #FFFFFF";></div>' +
                    '<div style="background-color: #FF0000";></div>' +
                    '<div style="background-color: #00FF00";></div>' +
                    '<div style="background-color: #0000FF";></div>').
                on('click', '.cp-memory div', function(e) {
                    var $this = $(this);

                    if (this.className) {
                        $this.parent().prepend($this.prev()).children().eq(0).
                            css('background-color', '#' + colorInstance.colors.HEX);
                    } else {
                        colorInstance.setColor($this.css('background-color'));
                        colorPicker.render();
                    }
                });
                this.$colorPatch = $elm.prepend('<div class="cp-disp">').find('.cp-disp');
            },

            cssAddon:
                '.cp-memory {margin-bottom:6px; clear:both;}' +
                '.cp-memory div {float:left; width:25%; height:40px;' +
                'background:rgba(0,0,0,1); text-align:center; line-height:40px;}' +
                '.cp-disp{padding:10px; margin-bottom:6px; font-size:19px; height:40px; line-height:20px}' +
                '.cp-xy-slider{width:200px; height:200px;}' +
                '.cp-xy-cursor{width:16px; height:16px; border-width:2px; margin:-8px}' +
                '.cp-z-slider{height:200px; width:40px;}' +
                '.cp-z-cursor{border-width:8px; margin-top:-8px;}',

            opacity: false,

            renderCallback: function($elm, toggled) {
                var colors = this.color.colors.RND;
                var json = {
                        'r': colors.rgb.r,
                        'g': colors.rgb.g,
                        'b': colors.rgb.b
                    };

                this.$colorPatch.css({
                    backgroundColor: '#' + colors.HEX,
                    color: colors.RGBLuminance > 0.22 ? '#222' : '#ddd'
                }).text(this.color.toString($elm._colorMode)); // $elm.val();

                ws.send('T1' + JSON.stringify(json));
            }
        });

        // Set page event feeds
        feed();
    });

    // DHCP field toggles
    $('#dhcp').click(function() {
        if ($(this).is(':checked')) {
            $('.dhcp').prop('disabled', true);
       } else {
            $('.dhcp').prop('disabled', false);
       }
    });

    // Pixel type toggles
    $('#p_type').change(function() {
        if ($('select[name_type]').val() == '1')
            $('#p_color').prop('disabled', true);
        else
            $('#p_color').prop('disabled', false);
    });

    // Serial protocol toggles
    $('#s_proto').change(function() {
        if ($('select[name=s_proto]').val() == '0')
            $('#s_baud').prop('disabled', true);
        else 
            $('#s_baud').prop('disabled', false);
    });




});

// Page event feeds
function feed() {
    if ($('#home').is(':visible')) {
        ws.send('X1');
        ws.send('X2');

        setTimeout(function() {
            feed();
        }, 1000);
    }
}

// WebSockets
function wsConnect() {
    if ('WebSocket' in window) {
        // Open a new web socket and set the binary type
        ws = new WebSocket('ws://' + document.location.host + '/ws');
        ws.binaryType = 'arraybuffer';

        ws.onopen = function() {
            ws.send('E1'); // Get html elements
            ws.send('G1'); // Get Config
            ws.send('G2'); // Get Net Status

            // Init dynamic fields
            $('#dhcp').trigger('click');
            $('#s_proto').trigger('change');

            feed();
        };
        
        ws.onmessage = function (event) { 
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
            case 'S1':
                setConfig(data);
                reboot();
                break;
            case 'S2':
                setConfig(data);
                break;
            case 'X1':
                getRSSI(data);
                break;
            case 'X2':
                getE131Status(data);
                break;
            case 'X6':
                showReboot();
                break;
            default:
                console.log('Unknown Command: ' + event.data);
                break;
            }
        };
        
        ws.onerror = function() {
            $('#wserror').modal({backdrop: 'static', keyboard: false});
        };

        ws.onclose = function() { 
            $('#wserror').modal({backdrop: 'static', keyboard: false});
        };
    } else {
        alert('WebSockets is NOT supported by your Browser! You will need to upgrade your browser or downgrade to v2.0 of the ESPixelStick firmware.');
    }
}

function getElements(data) {
    var elements = JSON.parse(data);

    for (var i in elements) {
        for (var j in elements[i]) {
            var opt = document.createElement('option');
            opt.text = j;
            opt.value = elements[i][j];
            document.getElementById(i).add(opt);
        }
    }
}

function getConfig(data) {
    var config = JSON.parse(data);

    // Device and Network config
    $('#title').text('ESPS - ' + config.device.id);
    $('#name').text(config.device.id);
    $('#devid').val(config.device.id);
    $('#ssid').val(config.network.ssid);
    $('#password').val(config.network.passphrase);
    $('#dhcp').prop('checked', config.network.dhcp);
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

    // E1.31 Config
    $('#universe').val(config.e131.universe);
    $('#channel_start').val(config.e131.channel_start);
    $('#multicast').prop('checked', config.e131.multicast);

    // Output Config
    $('.odiv').addClass('hidden');
    if (config.device.mode === 0) {  // Pixel
        mode = 'pixel';
        $('#o_pixel').removeClass('hidden');
        $('#p_count').val(config.e131.channel_count / 3);
        $('#p_type').val(config.pixel.type);
        $('#p_color').val(config.pixel.color);
        $('#p_gamma').prop('checked', config.pixel.gamma);
    }

    if (config.device.mode == 1) {  // Serial
        mode = 'serial';
        $('#o_serial').removeClass('hidden');
        $('#s_count').val(config.e131.channel_count);
        $('#s_proto').val(config.serial.type);
        $('#s_baud').val(config.serial.baudrate);
    }

    // Trigger updated elements
    $('#p_type').trigger('click');
    $('#p_count').trigger('change');
    $('#s_proto').trigger('click');
    $('#s_proto').trigger('change');
}

function getConfigStatus(data) {
    var status = JSON.parse(data);
    
    $('#x_ssid').text(status.ssid);
    $('#x_ip').text(status.ip);
    $('#x_mac').text(status.mac);
    $('#x_version').text(status.version);
}

function getRSSI(data) {
    var rssi = +data;
    var quality = 2 * (rssi + 100);

    if (rssi <= -100)
        quality = 0;
    else if (rssi >= -50)
        quality = 100;

    $('#x_rssi').text(rssi);
    $('#x_quality').text(quality);
}

function getE131Status(data) {
    var status = data.split(':');
   
    $('#uni_first').text(status[0]);
    $('#uni_last').text(status[1]);
    $('#pkts').text(status[2]);
    $('#serr').text(status[3]);
    $('#perr').text(status[4]);
}

function snackSave() {
    // Show snackbar for 3sec
    var x = document.getElementById('snackbar');
    x.className = 'show';
    setTimeout(function(){
        x.className = x.className.replace('show', ''); 
    }, 3000);    
}

function setConfig() {
    // Get config to refresh UI and show result
    ws.send("G1");
    snackSave();
}

function submitWiFi() {
    var ip = $('#ip').val().split('.');
    var netmask = $('#netmask').val().split('.');
    var gateway = $('#gateway').val().split('.');

    var json = {
            'network': {
                'ssid': $('#ssid').val(),
                'passphrase': $('#password').val(),
                'ip': [parseInt(ip[0]), parseInt(ip[1]), parseInt(ip[2]), parseInt(ip[3])],
                'netmask': [parseInt(netmask[0]), parseInt(netmask[1]), parseInt(netmask[2]), parseInt(netmask[3])],
                'gateway': [parseInt(gateway[0]), parseInt(gateway[1]), parseInt(gateway[2]), parseInt(gateway[3])],
                'dhcp': $('#dhcp').prop('checked'),
                'ap_fallback': $('#ap').prop('checked')
            }
        };
    ws.send('S1' + JSON.stringify(json));
}

function submitConfig() {
    var channels = parseInt($('#s_count').val());
    if (mode == 'pixel')
        channels = parseInt($('#p_count').val()) * 3;

    var json = {
            'device': {
                'id': $('#devid').val()
            },
            'e131': {
                'universe': parseInt($('#universe').val()),
                'channel_start': parseInt($('#channel_start').val()),
                'channel_count': channels,
                'multicast': $('#multicast').prop('checked')
            },
            'pixel': {
                'type': parseInt($('#p_type').val()),
                'color': parseInt($('#p_color').val()),
                'gamma': $('#p_gamma').prop('checked')
            },
            'serial': {
                'type': parseInt($('#s_proto').val()),
                'baudrate': parseInt($('#s_baud').val())
            }
        };
    ws.send('S2' + JSON.stringify(json));
}

function refreshPixel() {
    var proto = $('#p_type option:selected').text();    
    var size = parseInt($('#p_count').val());
    var frame = 30;
    var idle = 80;

    if (!proto.localeCompare('WS2811 800kHz')) {
        frame = 30;
        idle = 80;
    } else if (!proto.localeCompare('GE Color Effects')) {
        frame = 790;
        idle = 35;
    }

    var rate = (frame * size + idle) / 1000;
    var hz = 1000 / rate;
    $('#refresh').html(Math.ceil(rate) + 'ms / ' + Math.floor(hz) + 'Hz');
}

function refreshSerial() {
    var proto = $('#s_proto option:selected').text();
    var baud = parseInt($('#s_baud').val());
    var size = parseInt($('#s_count').val());
    var symbol = 11;
    if (!proto.localeCompare('Renard')) {
        symbol = 10;
        size = size + 2;
    } else if (!proto.localeCompare('DMX512')) {
        symbol = 11;
        baud = 250000;
        $('#s_baud').val(baud);
    }
    var rate = symbol * 1000 / baud * size;
    var hz = 1000 / rate;
    $('#refresh').html(Math.ceil(rate) + 'ms / ' + Math.floor(hz) + 'Hz');
}

function test() {
    console.log('test mode');
}

function showReboot() {
    $('#update').modal('hide');
    $('#reboot').modal({backdrop: 'static', keyboard: false});
    setTimeout(function() {
        window.location.replace("/"); 
    }, 5000);    
}

function reboot() {
    showReboot();
    ws.send('X6');
}