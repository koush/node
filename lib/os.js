// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var binding = process.binding('os');
var util = require('util');

exports.hostname = binding.getHostname;
exports.loadavg = binding.getLoadAvg;
exports.uptime = binding.getUptime;
exports.freemem = binding.getFreeMem;
exports.totalmem = binding.getTotalMem;
exports.cpus = binding.getCPUs;
exports.type = binding.getOSType;
exports.release = binding.getOSRelease;
exports.networkInterfaces = binding.getInterfaceAddresses;


exports.setupTun = function(cb) {
  var fs = require('fs');
  var os = require('os');
  if (os.platform() == 'darwin') {
    fs.open('/dev/tun1', 'r+', cb);
  }
  else if (os.platform() == 'linux') {
    fs.open('/dev/net/tun', 'r+', function(err, fd) {
      if (err) {
        cb(err);
        return;
      }
      err = binding.setupTun(fd);
      if (err) {
        fs.close(fd);
        cb(err);
        return;
      }
      cb(null, fd);
    })
  }
  else if (os.platform() == 'win32') {
    var fd = binding.setupTun();
    if (isNaN(fd)) {
      var err = fd;
      if (!err)
        err = 'Unknown error setting up tap device.';
      process.nextTick(function() {
        cb(err);
      });
      return;
    }
    process.nextTick(function() {
      cb(null, fd);
    });
  }
  else {
    process.nextTick(function() {
      cb('unsupported platform');
    });
  }
}

exports.arch = function() {
  return process.arch;
};

exports.platform = function() {
  return process.platform;
};

exports.tmpDir = function() {
  return process.env.TMPDIR ||
         process.env.TMP ||
         process.env.TEMP ||
         (process.platform === 'win32' ? 'c:\\windows\\temp' : '/tmp');
};

exports.getNetworkInterfaces = util.deprecate(function() {
  return exports.networkInterfaces();
}, 'getNetworkInterfaces is now called `os.networkInterfaces`.');

exports.EOL = process.platform === 'win32' ? '\r\n' : '\n';
