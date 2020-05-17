document.querySelectorAll('#nvs input').forEach((input) => {
    input.addEventListener('focusout', (event) => {
        fetch('/api/v1/nvs/' + event.target.dataset.namespace, {
            method: 'POST',
            headers: {
                      'Content-Type': 'application/json'
            },
            body: JSON.stringify({[event.target.name]: event.target.value}),
        }).then(function(response) {
            if(response.status != 200) {
                alert("Invalid Entry")
            }
        });
    })
})
