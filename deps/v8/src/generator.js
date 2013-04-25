function $Generator(value, resume) {
	this.value = value;
	this.resume = resume;
}
$Generator.prototype = $Object.prototype;
%SetProperty(global, "Generator", $Generator, DONT_ENUM);

// -------------------------------------------------------------------

function GeneratorNext() {
	if (this.resume) {
		var lastValue = this.value;
		var resume = this.resume;
		this.resume = null;
    resume();
		return lastValue;
	}
}

function SetUpGenerator() {
  %CheckIsBootstrapping();
  // %SetProperty($Generator.prototype, "constructor", $Generator, DONT_ENUM);

  // Set up non-enumerable functions of the Generator object and
  // set their names.
  InstallFunctions($Generator.prototype, DONT_ENUM, $Array(
		"next", GeneratorNext
  ));
}

SetUpGenerator();
