sed '/{JAVASCRIPT}/{
    s/{JAVASCRIPT}//g
    r blinds.js
}' blinds.html | ./node_modules/.bin/htmlmin -o blinds.min.html