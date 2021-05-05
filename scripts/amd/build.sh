set -e
cd python
pip uninstall -y triton 
pip install --verbose -e .