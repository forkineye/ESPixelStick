const express = require("express");
const app = express();
const espsIp = "http://192.168.10.167/";

app.listen(9000, () => {
    console.log("Application started and Listening on port 9000");
});

app.use(express.static(__dirname));

app.get("/", (req, res) => {
    res.sendFile(__dirname + "/index.html");
});


app.post("/XP",          (req, res) => {res.redirect(espsIp + "XP")});
app.get("/XJ",           (req, res) => {res.redirect(espsIp + "XJ")});
app.put("/conf/:file",   (req, res) => {res.redirect(espsIp + "conf/" + req.params.file);});
app.get("/conf/:file",   (req, res) => {res.redirect(espsIp + "conf/" + req.params.file);});
app.post("/settime",     (req, res) => {res.redirect(espsIp + "settime?newtime=" + req.query.newtime);});
app.get("/fseqfilelist", (req, res) => {res.redirect(espsIp + "fseqfilelist");});
