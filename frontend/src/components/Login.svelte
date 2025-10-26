<script>
  let username = $state('');
  let password = $state('');
  let error = '';

  async function handleLogin() {
      const response = await fetch('/api/login', {
          method: 'POST',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({username, password})
      });

      if (response.ok) {
          localStorage.setItem('username', username);
          localStorage.setItem('isLoggedIn', 'true');
          window.location.href = '/main';
      } else {
          error = 'Login failed';
      }
  }
</script>

<style>
    .input-group {
        display: flex;
        align-items: center;
        gap: 10px;
        margin-bottom: 10px;
    }

    .input-group label {
        width: 80px;
        text-align: right;
    }

    .input-group input {
        flex: 1;
        padding: 5px;
    }

    .button {
        margin-top: 10px;
        margin-bottom: 10px;
    }
</style>

<h1>Authorization</h1>

<form on:submit|preventDefault={handleLogin}>
    <div class="input-group">
        <label>Login</label>
        <input bind:value={username} />
    </div>

    <div class="input-group">
        <label>Password</label>
        <input bind:value={password} />
    </div>

    <button class="button" type="submit" disabled={!username || !password}>Log in</button>
</form>

<button>Only view</button>
