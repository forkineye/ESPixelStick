const express = require("express");
const app = express();

app.listen(9000, () => {
    console.log("Application started and Listening on port 9000");
});

app.use(express.static(__dirname));

app.get("/", (req, res) => {
    res.sendFile(__dirname + "/index.html");
});
