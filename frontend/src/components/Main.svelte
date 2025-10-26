<script>
    let username = '';

    if(typeof window !== 'undefined') {
        const user = localStorage.getItem('username');
        if (!user) {
            window.location.href = '/login';
        } else {
            username = user;
        }
    }
</script>

<h1>Welcome, {username}!</h1>

<button on:click={() => {
    localStorage.removeItem('isLoggedIn');
    window.location.href = '/login';
    }}>Log Out</button>/