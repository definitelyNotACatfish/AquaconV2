function Wartung_in_use(a) {
    if (a) {
        document.getElementById("Wartung_Si").checked = true;
        document.getElementById("PWM0-5").removeAttribute("disabled");
    } else {
        document.getElementById("Wartung_Si").checked = false;
        document.getElementById("PWM0-5").setAttribute("disabled", "disabled");
    }
}

function Wartung() {
    var a = new XMLHttpRequest();
    a.open("POST", "../sites/wartung.psf", true);
    if (document.getElementById("Wartung_Si").checked) {
        a.send("Wartung=True");
        document.getElementById("PWM0-5").removeAttribute("disabled");
    } else {
        a.send("Wartung=False");
        document.getElementById("PWM0-5").setAttribute("disabled", "disabled");
    }
}

function Set_PWM(a) {
    var b = new XMLHttpRequest();
    b.open("POST", "../sites/wartung.psf", true);
    switch (a) {
        case "PWM0_0":
            b.send("PWM0_0=" + document.getElementById("PWM0").value);
            break;
        case "PWM1_0":
            b.send("PWM1_0=" + document.getElementById("PWM1").value);
            break;
        case "PWM2_0":
            b.send("PWM2_0=" + document.getElementById("PWM2").value);
            break;
        case "PWM3_0":
            b.send("PWM3_0=" + document.getElementById("PWM3").value);
            break;
        case "PWM4_0":
            b.send("PWM4_0=" + document.getElementById("PWM4").value);
            break;
        case "PWM5_0":
            b.send("PWM5_0=" + document.getElementById("PWM5").value);
            break;
        default:
            break;
    }
}

function Get_ADCs()
{
    var a = new XMLHttpRequest();
    a.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            var c = [];
            c = a.responseText.split(",");
            c.pop();
            for (var b = 0; b < c.length; b += 2) {
                document.getElementById(c[b]).innerHTML = parseFloat(c[b + 1]).toFixed(3) + " V";
            }
        }
    };
    a.open("GET", "/uc/ADCs.con", true);
    a.send()
}

function Get_Power()
{
    var a = new XMLHttpRequest();
    a.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            var c = [];
            c = a.responseText.split(",");
            c.pop();
            for (var b = 0; b < c.length; b += 2) {
                document.getElementById(c[b]).innerHTML = c[b + 1];
            }
        }
    };
    a.open("GET", "/uc/Power.con", true);
    a.send()
}

function Modal_load()
{
    var c = document.getElementById("myModal");
    var a = document.getElementById("myBtn");
    var b = document.getElementsByClassName("close")[0];
    a.onclick = function() {
        c.style.display = "block";
    };
    b.onclick = function() {
        c.style.display = "none";
    };
    window.onclick = function(d) {
        if (d.target == c) {
            c.style.display = "none";
        }
    }
}

function Calculate_Filterstatus()
{
	var g = parseFloat(document.getElementById("ADC3").innerHTML);
	g = (g/0.33)*60;
	document.getElementById("Filterstate").innerHTML = g.toFixed(0) + " l/h (Leerlauf: 1200 l/h)";	
}

function Calculate_CO2()
{
	var g = parseFloat(document.getElementById("ADC2").innerHTML);
	var start = 2857;
	var end = 3357;
	var real = g;
	if((g*1000)>end){g=end/1000;}
	if((g*1000)<start){g=start/1000;}
		
	var pp = ((g*1000)-start)*((100-0)/(end-start));
	document.getElementById("CO2").innerHTML = pp.toFixed(0)+"% (" + (real*1000).toFixed(0) + "g)";	
}

function Calculate_Height()
{
	var g = parseFloat(document.getElementById("ADC1").innerHTML);
	var start = 0;
	var end = 10;
	var real = g;
		
	var pp = (g-start)*((45.4-0)/(end-start))+9.3;
	document.getElementById("Height").innerHTML = pp.toFixed(1)+"cm";	
}