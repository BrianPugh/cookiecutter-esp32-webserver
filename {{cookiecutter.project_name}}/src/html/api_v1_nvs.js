var tbl = document.getElementById('nvs')
tbl.addEventListener('focusout', function(){postEdits(event)}, false);

function postEdits()
{
  var col = window.event.target.cellIndex;
  var row = window.event.target.parentNode.rowIndex;
  if(col == 2) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                document.open();
                document.write(xhttp.responseText);
                document.close();
            } else if (xhttp.status == 0) {
                alert("Server closed the connection abruptly!");
                location.reload()
            } else {
                alert(xhttp.status + " Error!\n" + xhttp.responseText);
                location.reload()
            }
        }
    };
    var resp = {};
    resp[tbl.rows[row].cells[1].innerHTML] =  tbl.rows[row].cells[col].innerHTML;

    xhttp.open("POST", '/api/v1/nvs/' + tbl.rows[row].cells[0].innerHTML, true);
    xhttp.send(JSON.stringify(resp));
  }
}
