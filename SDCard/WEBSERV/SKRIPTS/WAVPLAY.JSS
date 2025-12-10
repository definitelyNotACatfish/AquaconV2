function WAV_Send_CMD(cmd)
{
	var xhttp=new XMLHttpRequest();
	xhttp.open("POST", "../sites/wavplay.psf", true);			// HTTP POST
	xhttp.send("WAV=Play " + cmd);
}