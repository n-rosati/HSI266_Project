getNewData()
setInterval(getNewData, 5000)

function getNewData() {
    const xhttp = new XMLHttpRequest();
    xhttp.onload = function () {
        let tableBody = document.getElementById("logTableBody");

        // Clear table first
        while (tableBody.hasChildNodes()) tableBody.removeChild(tableBody.firstChild)
        // ignore the first row, it's a header
        let data = CSVToArray(this.responseText, ',').slice(1).reverse()
        data.forEach((e) => {
            let row = tableBody.insertRow();

            let date = row.insertCell(0);
            let time = row.insertCell(1);
            let mode = row.insertCell(2);
            let value = row.insertCell(3);

            date.innerHTML = `${e[0]}-${e[1]}-${e[2]}`
            time.innerHTML = `${e[3]}:${e[4]}:${e[5]}`
            mode.innerHTML = e[6]
            value.innerHTML = e[7];

        })
    }
    xhttp.open("GET", "data.csv", true);
    xhttp.send();
}

// thanks Ben Nadel for this
// https://www.bennadel.com/blog/1504-ask-ben-parsing-csv-strings-with-javascript-exec-regular-expression-command.htm
function CSVToArray( strData, strDelimiter ){
    // Check to see if the delimiter is defined. If not,
    // then default to comma.
    strDelimiter = (strDelimiter || ",");

    // Create a regular expression to parse the CSV values.
    var objPattern = new RegExp(
        (
            // Delimiters.
            "(\\" + strDelimiter + "|\\r?\\n|\\r|^)" +

            // Quoted fields.
            "(?:\"([^\"]*(?:\"\"[^\"]*)*)\"|" +

            // Standard fields.
            "([^\"\\" + strDelimiter + "\\r\\n]*))"
        ),
        "gi"
    );


    // Create an array to hold our data. Give the array
    // a default empty first row.
    var arrData = [[]];

    // Create an array to hold our individual pattern
    // matching groups.
    var arrMatches = null;


    // Keep looping over the regular expression matches
    // until we can no longer find a match.
    while (arrMatches = objPattern.exec( strData )){

        // Get the delimiter that was found.
        var strMatchedDelimiter = arrMatches[ 1 ];

        // Check to see if the given delimiter has a length
        // (is not the start of string) and if it matches
        // field delimiter. If id does not, then we know
        // that this delimiter is a row delimiter.
        if (
            strMatchedDelimiter.length &&
            (strMatchedDelimiter != strDelimiter)
        ){

            // Since we have reached a new row of data,
            // add an empty row to our data array.
            arrData.push( [] );

        }


        // Now that we have our delimiter out of the way,
        // let's check to see which kind of value we
        // captured (quoted or unquoted).
        if (arrMatches[ 2 ]){

            // We found a quoted value. When we capture
            // this value, unescape any double quotes.
            var strMatchedValue = arrMatches[ 2 ].replace(
                new RegExp( "\"\"", "g" ),
                "\""
            );

        } else {

            // We found a non-quoted value.
            var strMatchedValue = arrMatches[ 3 ];

        }


        // Now that we have our value string, let's add
        // it to the data array.
        arrData[ arrData.length - 1 ].push( strMatchedValue );
    }

    // Return the parsed data.
    return( arrData );
}