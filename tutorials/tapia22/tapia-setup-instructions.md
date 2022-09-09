# Setup instructions

1. **ssh to AWS**

If you'd like to ssh to our AWS environment from a native terminal application, you can run `ssh <username>@18.219.49.4` with one of our provided usernames.

All usernames are of the form `user<N>`with corresponding passwords `user<N>PASS`.

For example, you might connect with

```
ssh user15@18.219.49.4
```

When prompted for a password, `user15` would provide `user15PASS`.

2. **Run setup script**

Once you've ssh'ed to AWS, you'll want to run the script `/home/tutorial/aws/user-env.sh` to set up your environment via

```
source /home/tutorial/aws/user-env.sh
```
