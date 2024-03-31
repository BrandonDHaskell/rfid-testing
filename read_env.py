Import("env")
import os

print(env.Dump())
# print(projenv.Dump())

def load_dotenv():
    dotenv_path = os.path.join(env.get("PROJECT_DIR"), ".env")
    with open(dotenv_path) as file:
        for line in file:
            if line.startswith('#') or not line.strip():
                continue
            key, value = line.strip().split('=', 1)
            value = value.strip('\'"')  # Remove possible quotes
            print(f"Setting {key} to {value}")
            env.Append(CPPDEFINES=[(key, f'\\"{value}\\"')])
        
        env.Append(CPPDEFINES=[("TEST_MACRO", '"test_macro"')])
        env.Append(CPPDEFINES=[("TEST_MACRO1", '\"\\\"test_macro\"\\\"')])
        env.Append(CPPDEFINES=[("TEST_MACRO2", '\\"test_macro\\"')])
        env.Append(CPPDEFINES=[("TEST_MACRO3", '\"test_macro\"')])
        print(env["CPPDEFINES"][4])

load_dotenv()