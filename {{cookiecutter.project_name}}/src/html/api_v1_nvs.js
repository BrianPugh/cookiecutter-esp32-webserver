document.querySelectorAll('#nvs input').forEach((input) => {
    input.addEventListener('blur', (event) => {
        fetch('/api/v1/nvs/', {
            method: 'POST',
            body: JSON.stringify(event.target.value),
        });
    })
})
