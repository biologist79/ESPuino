const express = require("express");
const app = express();

app.use(express.static("../../html"));

app.get("/", (req, res)=>{
    res.redirect("/management.html");
});

const server = app.listen(8081, ()=>{
    const host = server.address().address;
    const port = server.address().port;

    console.log("Test server listening at http://%s:%s", host, port);
})