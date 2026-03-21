'use strict';
'require view';
'require form';
'require uci';
'require rpc';
'require ui';
'require fs';

var callServiceList = rpc.declare({
	object: 'service',
	method: 'list',
	params: [ 'name' ],
	expect: { '' : {} }
});

return view.extend({
	load: function() {
		return Promise.all([
			callServiceList('mbusd'),
			fs.list('/etc/rc.d').then(function(entries) {
				return entries.some(function(e) {
					return e.name.indexOf('mbusd') !== -1;
				});
			}).catch(function() { return false; }),
			uci.load('mbusd').then(function() {
				return uci.get('mbusd', '@mbusd[0]', 'enabled') === '1';
			}),
			fs.exec('sh', ['-c', 'pgrep mbusd | while read p; do tr "\\0" " " < /proc/$p/cmdline; done'
			]).then(function(res) {
				return res.stdout ? res.stdout.trim() : null;
			}).catch(function() { return null; }),
			fs.exec('sh', ['-c', 'ip -4 addr show eth0 | grep -oE "inet [0-9.]+" | sed "s/inet //"'
			]).then(function(res) {
    				return res.stdout ? res.stdout.trim() : null;
			}).catch(function() { return null; })
		]);
	},

	render: function(data) {
		var m, s, o;
		var isEnabled = data[1];
		var isUciEnabled = data[2];
		var cmdline = data[3];
		var ipaddr = data[4];

		var isRunning = false;
		try {
			isRunning = Object.keys(data[0].mbusd.instances || {}).length > 0;
		} catch(e) {
			isRunning = false;
		}

		m = new form.Map('mbusd', _('Modbus Gateway'),
			_('Configuration for mbusd - Modbus TCP to RTU/ASCII gateway. To save settings press "Save & Apply".'));

		m.chain('mbusd');

		s = m.section(form.TypedSection, 'mbusd', _('Service Control'));
		s.anonymous = true;
		s.addremove = false;

		/* Status */
		o = s.option(form.DummyValue, '_status', _('Status'));
		o.rawhtml = true;
		o.default = isRunning
			? '<span style="color:green;font-weight:bold;">&#9679; ' + _('Running') + '</span>'
			: '<span style="color:red;font-weight:bold;">&#9679; ' + _('Stopped') + '</span>';

		/* Process status */
		o = s.option(form.DummyValue, '_cmdline', _('Process status'));
		o.rawhtml = true;
		o.default = cmdline
			? '<code style="font-size:0.85em;color:green;">' + cmdline + '</code>'
			: '<span style="color:grey;">-</span>';

		/* Listening on */
		o = s.option(form.DummyValue, '_listen', _('Listening on'));
		o.rawhtml = true;
		o.default = (isRunning && ipaddr)
			? '<span style="color:green;">' + ipaddr + ':' + (uci.get('mbusd', '@mbusd[0]', 'port') || '502') + '</span>'
			: '<span style="color:grey;">-</span>';

		/* Start/Stop */
		o = s.option(form.Button, '_startstop', _('Service'));
		o.inputstyle = isRunning ? 'reset' : 'apply';
		o.inputtitle = isRunning ? _('Stop') : _('Start');
		o.onclick = function() {
			var action = isRunning ? 'stop' : 'start';
			return fs.exec('/etc/init.d/mbusd', [action]).then(function() {
				ui.addNotification(null, E('p', _('Service ') + action + _('ed')), 'info');
				window.setTimeout(function() { window.location.reload(); }, 1000);
			});
		};

		/* Restart */
		o = s.option(form.Button, '_restart', _('Restart'));
		o.inputstyle = 'reload';
		o.inputtitle = _('Restart');
		o.onclick = function() {
			return fs.exec('/etc/init.d/mbusd', ['restart']).then(function() {
				ui.addNotification(null, E('p', _('Service restarted')), 'info');
				window.setTimeout(function() { window.location.reload(); }, 1000);
			});
		};

		/* Autostart status */
		o = s.option(form.DummyValue, '_autostart', _('Autostart'));
		o.rawhtml = true;
		o.default = isEnabled
			? '<span style="color:green;">&#9679; ' + _('Enabled') + '</span>'
			: '<span style="color:grey;">&#9679; ' + _('Disabled') + '</span>';

		/* Autostart button */
		o = s.option(form.Button, '_enabledisable', _('Autostart control'));
		o.inputstyle = isEnabled ? 'reset' : 'apply';
		o.inputtitle = isEnabled ? _('Disable autostart') : _('Enable autostart');
		o.onclick = function() {
			var action = isEnabled ? 'disable' : 'enable';
			return uci.load('mbusd').then(function() {
				uci.set('mbusd', '@mbusd[0]', 'enabled', isEnabled ? '0' : '1');
				return uci.save();
			}).then(function() {
				return uci.apply();
			}).then(function() {
				return fs.exec('/etc/init.d/mbusd', [action]);
			}).then(function() {
				ui.addNotification(null, E('p', _('Autostart ') + action + _('d')), 'info');
				window.setTimeout(function() { window.location.reload(); }, 1000);
			});
		};

		s = m.section(form.TypedSection, 'mbusd', _('General Settings'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Flag, 'enabled', _('Enable on boot'));
		o.rmempty = false;

		o = s.option(form.Value, 'port', _('TCP Port'),
			_('Modbus TCP listening port (default: 502)'));
		o.datatype = 'port';
		o.placeholder = '502';
		o.rmempty = false;

		o = s.option(form.ListValue, 'loglevel', _('Log Level'));
		o.value('0', _('0 - None'));
		o.value('1', _('1 - Error'));
		o.value('2', _('2 - Warning'));
		o.value('3', _('3 - Info'));
		o.value('4', _('4 - Debug'));
		o.default = '2';

		o = s.option(form.Value, 'max_connections', _('Max Connections'));
		o.datatype = 'uinteger';
		o.placeholder = '32';
		o.optional = true;

		o = s.option(form.Value, 'timeout', _('Timeout (ms)'));
		o.datatype = 'uinteger';
		o.placeholder = '60';
		o.optional = true;

		s = m.section(form.TypedSection, 'mbusd', _('Serial Port Settings'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Value, 'device', _('Serial Device'));
		o.placeholder = '/dev/ttyS1';
		o.rmempty = false;
		o.value('/dev/ttyUSB0', '/dev/ttyUSB0');
		o.value('/dev/ttyUSB1', '/dev/ttyUSB1');
		o.value('/dev/ttyS0', '/dev/ttyS0');
		o.value('/dev/ttyS1', '/dev/ttyS1');
		o.value('/dev/ttyS2', '/dev/ttyS2');
		o.value('/dev/ttyS3', '/dev/ttyS3');

		o = s.option(form.ListValue, 'speed', _('Baud Rate'));
		o.value('1200',   '1200');
		o.value('2400',   '2400');
		o.value('4800',   '4800');
		o.value('9600',   '9600');
		o.value('19200',  '19200');
		o.value('38400',  '38400');
		o.value('57600',  '57600');
		o.value('115200', '115200');
		o.default = '115200';

		o = s.option(form.ListValue, 'databits', _('Data Bits'));
		o.value('7', '7');
		o.value('8', '8');
		o.default = '8';

		o = s.option(form.ListValue, 'parity', _('Parity'));
		o.value('N', _('None'));
		o.value('E', _('Even'));
		o.value('O', _('Odd'));
		o.default = 'N';

		o = s.option(form.ListValue, 'stopbits', _('Stop Bits'));
		o.value('1', '1');
		o.value('2', '2');
		o.default = '1';

		o = s.option(form.ListValue, 'rts', _('RTS Mode'),
			_('RTS signal control for RS485 direction switching'));
		o.value('0', _('0 - None'));
		o.value('1', _('1 - Up before send'));
		o.value('2', _('2 - Down before send'));
		o.default = '0';

		o = s.option(form.Value, 'rtu_retries', _('RTU Retries'));
		o.datatype = 'uinteger';
		o.placeholder = '3';
		o.optional = true;

		o = s.option(form.Value, 'rtu_wait', _('RTU Wait (ms)'));
		o.datatype = 'uinteger';
		o.placeholder = '500';
		o.optional = true;

		return m.render();
	}
});
