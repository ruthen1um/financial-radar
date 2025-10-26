<script>
    let currentPage = 'login';

    let username = '';
    let password = '';
    let error = '';

    async function handleLogin() {
        error = '';

        const response = await fetch('/api/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, password })
        });

        if (response.ok) {
            localStorage.setItem('username', username);
            currentPage = 'main';
        } else {
            error = 'Login failed';
        }
    }

    function handleLogout() {
        localStorage.removeItem('username');
        currentPage = 'login';
        username = '';
        password = '';
    }

    if (typeof window !== 'undefined') {
        const savedUser = localStorage.getItem('username');
        if (savedUser) {
            username = savedUser;
            currentPage = 'main';
        }
    }
</script>

{#if currentPage === 'login'}
    <div style="max-width: 400px; margin: 50px auto; padding: 20px;">
        <h2>Authorization</h2>
        <form on:submit|preventDefault={handleLogin}>
            <div style="margin-bottom: 12px;">
                <label style="display: block; margin-bottom: 4px;">Login:</label>
                <input
                        type="text"
                        bind:value={username}
                        style="width: 100%; padding: 6px; border: 1px solid #999;"
                />
            </div>
            <div style="margin-bottom: 12px;">
                <label style="display: block; margin-bottom: 4px;">Password:</label>
                <input
                        type="password"
                        bind:value={password}
                        style="width: 100%; padding: 6px; border: 1px solid #999;"
                />
            </div>
            {#if error}
                <p style="color: red; margin-bottom: 12px;">{error}</p>
            {/if}
            <button
                    type="submit"
                    disabled={!username || !password}
                    style="padding: 8px 16px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer;"
            >
                Log in
            </button>
        </form>
    </div>

{:else if currentPage === 'main'}
    <div style="max-width: 600px; margin: 50px auto; padding: 20px; text-align: center;">
        <h2>Welcome, {username}!</h2>
        <button
                on:click={handleLogout}
                style="padding: 8px 16px; background: #6c757d; color: white; border: none; border-radius: 4px; cursor: pointer; margin-top: 20px;"
        >
            Log Out
        </button>
    </div>
{/if}