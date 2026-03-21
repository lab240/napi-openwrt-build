'use strict';
'require view';
'require dom';
'require fs';
'require ui';

/*
 * luci-app-mbpoll — Modbus Poll & Scan Tool
 *
 * Tab 1: Poll — manual RTU/TCP register read via mbpoll
 * Tab 2: Scan — fast RTU bus device discovery via mbscan (C utility)
 */

/* ---- data type helpers ---- */

function parseUint16(arr, idx) { return arr[idx] || 0; }

function parseInt16(arr, idx) {
	var v = parseUint16(arr, idx);
	return v > 32767 ? v - 65536 : v;
}

function parseUint32(arr, idx) {
	if (idx + 1 >= arr.length) return null;
	return (arr[idx] << 16) | arr[idx + 1];
}

function parseInt32(arr, idx) {
	var v = parseUint32(arr, idx);
	if (v === null) return null;
	return v > 2147483647 ? v - 4294967296 : v;
}

function parseFloat32(arr, idx) {
	if (idx + 1 >= arr.length) return null;
	var buf = new ArrayBuffer(4);
	var dv = new DataView(buf);
	dv.setUint16(0, arr[idx]);
	dv.setUint16(2, arr[idx + 1]);
	return dv.getFloat32(0);
}

function parseFloat32sw(arr, idx) {
	if (idx + 1 >= arr.length) return null;
	var buf = new ArrayBuffer(4);
	var dv = new DataView(buf);
	dv.setUint16(0, arr[idx + 1]);
	dv.setUint16(2, arr[idx]);
	return dv.getFloat32(0);
}

function hex16(v) {
	return '0x' + ('0000' + v.toString(16).toUpperCase()).slice(-4);
}

function fmtFloat(v) {
	if (v === null) return '\u2014';
	if (!isFinite(v)) return String(v);
	return v.toFixed(4);
}

/* ---- detect serial ports ---- */

function detectPorts() {
	return fs.exec('/bin/sh', ['-c',
		'ls /dev/ttyS[0-9]* /dev/ttyUSB[0-9]* /dev/ttyACM[0-9]* 2>/dev/null'
	]).then(function(res) {
		var ports = [];
		if (res && res.stdout) {
			res.stdout.trim().split('\n').forEach(function(p) {
				p = p.trim();
				if (p && p !== '/dev/ttyS0')
					ports.push(p);
			});
		}
		if (ports.length === 0)
			ports = ['/dev/ttyS1', '/dev/ttyS2'];
		return ports;
	}).catch(function() {
		return ['/dev/ttyS1', '/dev/ttyS2'];
	});
}

/* ---- parse mbpoll stdout ---- */

function parseMbpollOutput(stdout) {
	var registers = [];
	var lines = stdout.split('\n');
	for (var i = 0; i < lines.length; i++) {
		var m = lines[i].trim().match(/^\[(\d+)\]:\s+(\d+)/);
		if (m) {
			registers.push({
				addr: parseInt(m[1], 10),
				value: parseInt(m[2], 10)
			});
		}
	}
	return registers;
}

/* ---- parse mbscan stdout ---- */

function parseMbscanOutput(stdout) {
	var devices = [];
	var lines = stdout.split('\n');
	for (var i = 0; i < lines.length; i++) {
		var line = lines[i].trim();
		/* Format: "Found slave 125: [0]=125 [1]=1 [2]=830 [3]=794" */
		var m = line.match(/^Found slave\s+(\d+):(.*)/);
		if (m) {
			var addr = parseInt(m[1], 10);
			var regsStr = m[2].trim();
			var regs = [];
			var re = /\[(\d+)\]=(\d+)/g;
			var rm;
			while ((rm = re.exec(regsStr)) !== null) {
				regs.push({ reg: parseInt(rm[1], 10), val: parseInt(rm[2], 10) });
			}
			devices.push({ addr: addr, regs: regs });
		}
	}
	return devices;
}

/* ---- tab switching ---- */

function switchTab(tabId) {
	['poll', 'scan'].forEach(function(t) {
		var btn = document.getElementById('tab_btn_' + t);
		var pane = document.getElementById('tab_pane_' + t);
		if (t === tabId) {
			btn.classList.add('cbi-tab');
			btn.classList.remove('cbi-tab-disabled');
			pane.style.display = '';
		} else {
			btn.classList.remove('cbi-tab');
			btn.classList.add('cbi-tab-disabled');
			pane.style.display = 'none';
		}
	});
}

/* ---- RTU/TCP visibility ---- */

function updateModeVisibility() {
	var mode = document.getElementById('mb_mode').value;
	var isRtu = (mode === 'rtu');
	['mb_row_port', 'mb_row_serial_params', 'mb_row_stopbits'].forEach(function(id) {
		var el = document.getElementById(id);
		if (el) el.style.display = isRtu ? '' : 'none';
	});
	['mb_row_tcphost'].forEach(function(id) {
		var el = document.getElementById(id);
		if (el) el.style.display = isRtu ? 'none' : '';
	});
}

/* ---- helper: create serial port field (select + input) ---- */

function createPortField(ports, idPrefix) {
	var portSelect = E('select', {
		'id': idPrefix + '_port_select', 'class': 'cbi-input-select',
		'style': 'width:auto; margin-right:6px',
		'change': function() {
			document.getElementById(idPrefix + '_port').value = this.value;
		}
	}, ports.map(function(p) { return E('option', { 'value': p }, p); }));

	var portInput = E('input', {
		'id': idPrefix + '_port', 'type': 'text', 'class': 'cbi-input-text',
		'value': ports[0] || '/dev/ttyS1',
		'style': 'width:170px', 'placeholder': '/dev/ttyUSB0'
	});

	return E('span', {}, [
		portSelect,
		E('span', { 'style': 'margin:0 6px; color:#666' }, _('or')),
		portInput
	]);
}

/* ---- helper: baud select ---- */

function createBaudSelect(id) {
	return E('select', { 'id': id, 'class': 'cbi-input-select' },
		['1200','2400','4800','9600','19200','38400','57600','115200'].map(function(b) {
			var opt = E('option', { 'value': b }, b);
			if (b === '115200') opt.selected = true;
			return opt;
		}));
}

/* ---- helper: parity select ---- */

function createParitySelect(id) {
	return E('select', { 'id': id, 'class': 'cbi-input-select' },
		[['none','None'],['even','Even'],['odd','Odd']].map(function(p) {
			var opt = E('option', { 'value': p[0] }, p[1]);
			if (p[0] === 'none') opt.selected = true;
			return opt;
		}));
}

/* ==================================================================
 *  MAIN VIEW
 * ================================================================== */

return view.extend({
	load: function() {
		return detectPorts();
	},

	render: function(ports) {

		/* ---- Tab buttons ---- */
		var tabBar = E('ul', { 'class': 'cbi-tabmenu' }, [
			E('li', { 'id': 'tab_btn_poll', 'class': 'cbi-tab',
				'click': function() { switchTab('poll'); }
			}, [ E('a', { 'href': '#' }, _('Poll')) ]),
			E('li', { 'id': 'tab_btn_scan', 'class': 'cbi-tab-disabled',
				'click': function() { switchTab('scan'); }
			}, [ E('a', { 'href': '#' }, _('Scan Bus')) ])
		]);

		/* ==============================================================
		 *  TAB 1: POLL
		 * ============================================================== */

		var modeSelect = E('select', {
			'id': 'mb_mode', 'class': 'cbi-input-select',
			'change': function() { updateModeVisibility(); }
		}, [
			E('option', { 'value': 'rtu', 'selected': true }, 'Modbus RTU'),
			E('option', { 'value': 'tcp' }, 'Modbus TCP')
		]);

		var tcpHostInput = E('input', {
			'id': 'mb_tcphost', 'type': 'text', 'class': 'cbi-input-text',
			'value': '192.168.1.1', 'style': 'width:170px'
		});
		var tcpPortInput = E('input', {
			'id': 'mb_tcpport', 'type': 'number', 'class': 'cbi-input-text',
			'value': '502', 'min': '1', 'max': '65535', 'style': 'width:100px'
		});

		var fcSelect = E('select', { 'id': 'mb_fc', 'class': 'cbi-input-select' }, [
			E('option', { 'value': 'fc03' }, 'FC03 \u2014 Holding Registers'),
			E('option', { 'value': 'fc04' }, 'FC04 \u2014 Input Registers')
		]);

		var slaveInput = E('input', {
			'id': 'mb_slave', 'type': 'number', 'class': 'cbi-input-text',
			'value': '1', 'min': '1', 'max': '247', 'style': 'width:80px'
		});
		var startInput = E('input', {
			'id': 'mb_startreg', 'type': 'number', 'class': 'cbi-input-text',
			'value': '1', 'min': '1', 'max': '65535', 'style': 'width:100px'
		});
		var countInput = E('input', {
			'id': 'mb_count', 'type': 'number', 'class': 'cbi-input-text',
			'value': '10', 'min': '1', 'max': '125', 'style': 'width:80px'
		});
		var timeoutInput = E('input', {
			'id': 'mb_timeout', 'type': 'number', 'class': 'cbi-input-text',
			'value': '1', 'min': '1', 'max': '30', 'style': 'width:80px'
		});
		var dtSelect = E('select', { 'id': 'mb_datatype', 'class': 'cbi-input-select' },
			[['uint16','uint16'],['int16','int16'],
			 ['uint32','uint32 (AB CD)'],['int32','int32 (AB CD)'],
			 ['float32','float32 (AB CD)'],['float32sw','float32 (CD AB)']
			].map(function(d) { return E('option', { 'value': d[0] }, d[1]); })
		);

		var pollStatusEl = E('div', { 'id': 'poll_status', 'style': 'margin:8px 0; font-weight:bold; min-height:1.5em' });
		var pollResultEl = E('div', { 'id': 'poll_results' });
		var pollRawEl = E('div', { 'id': 'poll_raw', 'style': 'display:none' }, [
			E('h4', {}, _('Raw mbpoll output')),
			E('pre', { 'id': 'poll_raw_pre', 'style': 'background:#f5f5f5; padding:8px; max-height:200px; overflow:auto; font-size:12px' })
		]);

		var pollBtn = E('button', {
			'class': 'btn cbi-button cbi-button-action',
			'style': 'margin:10px 0',
			'click': doPoll
		}, [ _('Poll') ]);

		var pollForm = E('div', { 'class': 'cbi-section' }, [
			E('table', { 'class': 'table', 'style': 'max-width:720px' }, [
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td', 'colspan': '2' }, [
						E('label', {}, _('Mode')), E('br'), modeSelect
					])
				]),
				E('tr', { 'class': 'tr', 'id': 'mb_row_port' }, [
					E('td', { 'class': 'td', 'colspan': '2' }, [
						E('label', {}, _('Serial Port')), E('br'),
						createPortField(ports, 'mb')
					])
				]),
				E('tr', { 'class': 'tr', 'id': 'mb_row_serial_params' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Baud Rate')), E('br'), createBaudSelect('mb_baud')
					]),
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Data / Parity')), E('br'),
						E('select', { 'id': 'mb_databits', 'class': 'cbi-input-select' }, [
							E('option', { 'value': '7' }, '7'),
							E('option', { 'value': '8', 'selected': true }, '8')
						]),
						E('span', { 'style': 'margin:0 4px' }),
						createParitySelect('mb_parity')
					])
				]),
				E('tr', { 'class': 'tr', 'id': 'mb_row_stopbits' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Stop Bits')), E('br'),
						E('select', { 'id': 'mb_stopbits', 'class': 'cbi-input-select' }, [
							E('option', { 'value': '1', 'selected': true }, '1'),
							E('option', { 'value': '2' }, '2')
						])
					]),
					E('td', { 'class': 'td' })
				]),
				E('tr', { 'class': 'tr', 'id': 'mb_row_tcphost', 'style': 'display:none' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Host')), E('br'), tcpHostInput
					]),
					E('td', { 'class': 'td' }, [
						E('label', {}, _('TCP Port')), E('br'), tcpPortInput
					])
				]),
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Function')), E('br'), fcSelect
					]),
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Slave Address')), E('br'), slaveInput
					])
				]),
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Start Register')), E('br'), startInput
					]),
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Count')), E('br'), countInput
					])
				]),
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Timeout (sec)')), E('br'), timeoutInput
					]),
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Data Interpretation')), E('br'), dtSelect
					])
				])
			])
		]);

		var pollPane = E('div', { 'id': 'tab_pane_poll' }, [
			pollForm, pollBtn, pollStatusEl, pollResultEl, pollRawEl
		]);

		/* ==============================================================
		 *  TAB 2: SCAN BUS
		 * ============================================================== */

		var scanFromInput = E('input', {
			'id': 'scan_from', 'type': 'number', 'class': 'cbi-input-text',
			'value': '1', 'min': '1', 'max': '247', 'style': 'width:80px'
		});
		var scanToInput = E('input', {
			'id': 'scan_to', 'type': 'number', 'class': 'cbi-input-text',
			'value': '247', 'min': '1', 'max': '247', 'style': 'width:80px'
		});
		var scanTimeoutInput = E('input', {
			'id': 'scan_timeout', 'type': 'number', 'class': 'cbi-input-text',
			'value': '50', 'min': '5', 'max': '5000', 'style': 'width:80px'
		});
		var scanRegCount = E('input', {
			'id': 'scan_regcount', 'type': 'number', 'class': 'cbi-input-text',
			'value': '4', 'min': '1', 'max': '20', 'style': 'width:80px'
		});

		/* Data format selector for scan: combines databits + parity + stopbits */
		var scanFormatSelect = E('select', { 'id': 'scan_format', 'class': 'cbi-input-select' }, [
			E('option', { 'value': '8N1', 'selected': true }, '8N1'),
			E('option', { 'value': '8E1' }, '8E1'),
			E('option', { 'value': '8O1' }, '8O1'),
			E('option', { 'value': '8N2' }, '8N2'),
			E('option', { 'value': '7E1' }, '7E1'),
			E('option', { 'value': '7O1' }, '7O1')
		]);

		var scanStatusEl = E('div', { 'id': 'scan_status', 'style': 'margin:8px 0; font-weight:bold; min-height:1.5em' });
		var scanResultEl = E('div', { 'id': 'scan_results' });
		var scanRawEl = E('div', { 'id': 'scan_raw', 'style': 'display:none' }, [
			E('h4', {}, _('Raw mbscan output')),
			E('pre', { 'id': 'scan_raw_pre', 'style': 'background:#f5f5f5; padding:8px; max-height:200px; overflow:auto; font-size:12px' })
		]);

		var scanBtn = E('button', {
			'id': 'scan_btn',
			'class': 'btn cbi-button cbi-button-action',
			'style': 'margin:10px 0',
			'click': doScan
		}, [ _('Scan') ]);

		var scanForm = E('div', { 'class': 'cbi-section' }, [
			E('table', { 'class': 'table', 'style': 'max-width:720px' }, [
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td', 'colspan': '2' }, [
						E('label', {}, _('Serial Port')), E('br'),
						createPortField(ports, 'scan')
					])
				]),
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Baud Rate')), E('br'), createBaudSelect('scan_baud')
					]),
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Data Format')), E('br'), scanFormatSelect
					])
				]),
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Address Range')), E('br'),
						scanFromInput,
						E('span', { 'style': 'margin:0 6px' }, '\u2014'),
						scanToInput
					]),
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Timeout per address (ms)')), E('br'),
						scanTimeoutInput
					])
				]),
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td' }, [
						E('label', {}, _('Registers to read')), E('br'),
						scanRegCount
					]),
					E('td', { 'class': 'td' })
				])
			])
		]);

		var scanPane = E('div', { 'id': 'tab_pane_scan', 'style': 'display:none' }, [
			scanForm, scanBtn, scanStatusEl, scanResultEl, scanRawEl
		]);

		/* ==============================================================
		 *  POLL LOGIC
		 * ============================================================== */

		function doPoll() {
			var mode     = document.getElementById('mb_mode').value;
			var fc       = document.getElementById('mb_fc').value;
			var slave    = parseInt(document.getElementById('mb_slave').value, 10) || 1;
			var startreg = parseInt(document.getElementById('mb_startreg').value, 10) || 1;
			var count    = parseInt(document.getElementById('mb_count').value, 10) || 10;
			var timeout  = parseInt(document.getElementById('mb_timeout').value, 10) || 1;
			var datatype = document.getElementById('mb_datatype').value;

			if (slave < 1 || slave > 247) {
				pollStatusEl.textContent = _('Error: Slave address must be 1\u2013247');
				return;
			}
			if (count < 1 || count > 125) {
				pollStatusEl.textContent = _('Error: Count must be 1\u2013125');
				return;
			}

			var fcArg = fc === 'fc04' ? '3' : '4';
			var cmd = 'mbpoll';
			var target;

			if (mode === 'rtu') {
				target = document.getElementById('mb_port').value.trim();
				if (!target) { pollStatusEl.textContent = _('Error: Serial port is empty'); return; }
				cmd += ' -m rtu'
					+ ' -b ' + document.getElementById('mb_baud').value
					+ ' -d ' + document.getElementById('mb_databits').value
					+ ' -P ' + document.getElementById('mb_parity').value
					+ ' -s ' + document.getElementById('mb_stopbits').value;
			} else {
				target = document.getElementById('mb_tcphost').value.trim();
				var tcpport = document.getElementById('mb_tcpport').value.trim() || '502';
				if (!target) { pollStatusEl.textContent = _('Error: TCP host is empty'); return; }
				cmd += ' -m tcp -p ' + tcpport;
			}

			cmd += ' -a ' + slave + ' -t ' + fcArg + ' -r ' + startreg
				+ ' -c ' + count + ' -1 -o ' + timeout + ' ' + target;

			pollStatusEl.textContent = _('Polling...');
			pollStatusEl.style.color = '';
			pollResultEl.textContent = '';
			pollRawEl.style.display = 'none';
			pollBtn.disabled = true;

			fs.exec('/bin/sh', ['-c', cmd + ' 2>&1']).then(function(res) {
				pollBtn.disabled = false;
				var stdout = (res && res.stdout) ? res.stdout : '';
				var code = (res && res.code !== undefined) ? res.code : -1;

				document.getElementById('poll_raw_pre').textContent = '$ ' + cmd + '\n\n' + stdout;
				pollRawEl.style.display = '';

				if (code !== 0 || !stdout) {
					pollStatusEl.textContent = _('Error: mbpoll returned code ') + code;
					pollStatusEl.style.color = '#c00';
					return;
				}

				var regs = parseMbpollOutput(stdout);
				if (regs.length === 0) {
					pollStatusEl.textContent = _('No registers in response. Check connection and parameters.');
					pollStatusEl.style.color = '#c00';
					return;
				}

				pollStatusEl.textContent = _('OK \u2014 read ') + regs.length + _(' register(s) from slave ') + slave;
				pollStatusEl.style.color = '#080';

				var vals = regs.map(function(r) { return r.value; });
				var is32bit = (datatype === 'uint32' || datatype === 'int32' ||
				               datatype === 'float32' || datatype === 'float32sw');

				var thead = E('tr', { 'class': 'tr' }, [
					E('th', { 'class': 'th' }, _('Register')),
					E('th', { 'class': 'th' }, _('Raw (dec)')),
					E('th', { 'class': 'th' }, _('Raw (hex)')),
					E('th', { 'class': 'th' }, datatype)
				]);
				var rows = [thead];

				if (is32bit) {
					for (var i = 0; i < regs.length - 1; i += 2) {
						var interp;
						switch (datatype) {
							case 'uint32':    interp = parseUint32(vals, i); break;
							case 'int32':     interp = parseInt32(vals, i); break;
							case 'float32':   interp = fmtFloat(parseFloat32(vals, i)); break;
							case 'float32sw': interp = fmtFloat(parseFloat32sw(vals, i)); break;
						}
						rows.push(E('tr', { 'class': 'tr' }, [
							E('td', { 'class': 'td' }, regs[i].addr + '\u2013' + regs[i+1].addr),
							E('td', { 'class': 'td' }, regs[i].value + ', ' + regs[i+1].value),
							E('td', { 'class': 'td' }, hex16(regs[i].value) + ' ' + hex16(regs[i+1].value)),
							E('td', { 'class': 'td', 'style': 'font-weight:bold' }, String(interp))
						]));
					}
					if (regs.length % 2 !== 0) {
						var last = regs[regs.length - 1];
						rows.push(E('tr', { 'class': 'tr' }, [
							E('td', { 'class': 'td' }, String(last.addr)),
							E('td', { 'class': 'td' }, String(last.value)),
							E('td', { 'class': 'td' }, hex16(last.value)),
							E('td', { 'class': 'td', 'style': 'color:#999' }, _('(unpaired)'))
						]));
					}
				} else {
					for (var j = 0; j < regs.length; j++) {
						var v16 = (datatype === 'int16') ? parseInt16(vals, j) : parseUint16(vals, j);
						rows.push(E('tr', { 'class': 'tr' }, [
							E('td', { 'class': 'td' }, String(regs[j].addr)),
							E('td', { 'class': 'td' }, String(regs[j].value)),
							E('td', { 'class': 'td' }, hex16(regs[j].value)),
							E('td', { 'class': 'td', 'style': 'font-weight:bold' }, String(v16))
						]));
					}
				}

				dom.content(pollResultEl, E('table', { 'class': 'table' }, rows));

			}).catch(function(err) {
				pollBtn.disabled = false;
				pollStatusEl.textContent = _('Exec error: ') + String(err);
				pollStatusEl.style.color = '#c00';
			});
		}

		/* ==============================================================
		 *  SCAN LOGIC — calls mbscan binary (single exec, fast)
		 * ============================================================== */

		function doScan() {
			var port     = document.getElementById('scan_port').value.trim();
			var baud     = document.getElementById('scan_baud').value;
			var format   = document.getElementById('scan_format').value;
			var from     = parseInt(document.getElementById('scan_from').value, 10) || 1;
			var to       = parseInt(document.getElementById('scan_to').value, 10) || 247;
			var timeout  = parseInt(document.getElementById('scan_timeout').value, 10) || 50;
			var regcount = parseInt(document.getElementById('scan_regcount').value, 10) || 4;

			if (!port) {
				scanStatusEl.textContent = _('Error: Serial port is empty');
				return;
			}
			if (from < 1) from = 1;
			if (to > 247) to = 247;
			if (from > to) {
				scanStatusEl.textContent = _('Error: Invalid address range');
				return;
			}

			var cmd = 'mbscan'
				+ ' -p ' + port
				+ ' -b ' + baud
				+ ' -d ' + format
				+ ' -f ' + from
				+ ' -t ' + to
				+ ' -o ' + timeout
				+ ' -c ' + regcount;

			var total = to - from + 1;

			scanStatusEl.textContent = _('Scanning ') + total + _(' addresses...');
			scanStatusEl.style.color = '';
			scanResultEl.textContent = '';
			scanRawEl.style.display = 'none';
			scanBtn.disabled = true;

			fs.exec('/bin/sh', ['-c', cmd + ' 2>/tmp/.mbscan_log']).then(function(res) {
				scanBtn.disabled = false;
				var stdout = (res && res.stdout) ? res.stdout : '';
				var stderr = (res && res.stderr) ? res.stderr : '';

				/* Show raw output: command + results + log */
				var rawText = '$ ' + cmd + '\n\n' + stdout;
				document.getElementById('scan_raw_pre').textContent = rawText;
				scanRawEl.style.display = '';

				/* Parse devices */
				var devices = parseMbscanOutput(stdout);

				if (devices.length === 0) {
					scanStatusEl.textContent = _('Scan complete. No devices found in range ') + from + '\u2013' + to + '.';
					scanStatusEl.style.color = '#c00';
					return;
				}

				scanStatusEl.textContent = _('Scan complete. Found ') + devices.length + _(' device(s) in range ') + from + '\u2013' + to + '.';
				scanStatusEl.style.color = '#080';

				/* Build results table */
				var thead = E('tr', { 'class': 'tr' }, [
					E('th', { 'class': 'th' }, _('Slave Address')),
					E('th', { 'class': 'th' }, _('Registers')),
					E('th', { 'class': 'th' }, _('Status'))
				]);
				var rows = [thead];

				devices.forEach(function(d) {
					var regStr = d.regs.map(function(r) {
						return '[' + r.reg + ']=' + r.val;
					}).join('  ');

					rows.push(E('tr', { 'class': 'tr' }, [
						E('td', { 'class': 'td', 'style': 'font-weight:bold; font-size:1.1em' }, String(d.addr)),
						E('td', { 'class': 'td', 'style': 'font-family:monospace' }, regStr),
						E('td', { 'class': 'td', 'style': 'color:#080' }, '\u2713 Online')
					]));
				});

				dom.content(scanResultEl, E('table', { 'class': 'table' }, rows));

			}).catch(function(err) {
				scanBtn.disabled = false;
				scanStatusEl.textContent = _('Exec error: ') + String(err);
				scanStatusEl.style.color = '#c00';
			});
		}

		/* ==============================================================
		 *  ASSEMBLE PAGE
		 * ============================================================== */

		return E('div', {}, [
			E('h2', {}, _('Modbus Poll')),
			E('div', { 'class': 'cbi-map-descr' },
				_('Manual Modbus RTU/TCP polling and fast RTU bus scanning.')),
			tabBar,
			pollPane,
			scanPane
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
